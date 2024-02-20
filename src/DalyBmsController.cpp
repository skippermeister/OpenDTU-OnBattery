//#ifdef USE_DALYBMS_CONTROLLER

#include "DalyBmsController.h"
#include "Battery.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PinMapping.h"
#include <Arduino.h>
#include <ctime>
#include <frozen/map.h>

static constexpr char TAG[] = "[Daly BMS]";

bool DalyBmsController::init()
{
    _lastStatusPrinted.set(10 * 1000);

    if (!Configuration.get().Battery.Enabled)
        return false;

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("Initialize Daly BMS Controller rx = %d, tx = %d, rts = %d. ", pin.battery_rx, pin.battery_tx, pin.battery_rts);

    if (pin.battery_rx < 0 || pin.battery_tx < 0 || pin.battery_rts < 0) {
        MessageOutput.println("Invalid RX/TX/RTS pin config");
        return false;
    }

    if (!_stats->_initialized) {
        HwSerial.begin(9600, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
        HwSerial.setPins(pin.battery_rx, pin.battery_tx, UART_PIN_NO_CHANGE, pin.battery_rts);
        ESP_ERROR_CHECK(uart_set_mode(2, UART_MODE_RS485_HALF_DUPLEX));
        // Set read timeout of UART TOUT feature
        ESP_ERROR_CHECK(uart_set_rx_timeout(2, ECHO_READ_TOUT));

        HwSerial.flush();

        while (HwSerial.available()) { // clear RS485 read buffer
            HwSerial.read();
            vTaskDelay(1);
        }

        memset(_txBuffer, 0x00, XFER_BUFFER_LENGTH);
        clearGet();
        MessageOutput.println("initialized successfully.");

        _stats->_initialized = true;
    }

    return true;
}

void DalyBmsController::deinit()
{
    HwSerial.end();
}

frozen::string const& DalyBmsController::getStatusText(DalyBmsController::Status status)
{
    static constexpr frozen::string missing = "programmer error: missing status text";

    static constexpr frozen::map<Status, frozen::string, 6> texts = {
        { Status::Timeout, "timeout wating for response from BMS" },
        { Status::WaitingForPollInterval, "waiting for poll interval to elapse" },
        { Status::HwSerialNotAvailableForWrite, "UART is not available for writing" },
        { Status::BusyReading, "busy waiting for or reading a message from the BMS" },
        { Status::RequestSent, "request for data sent" },
        { Status::FrameCompleted, "a whole frame was received" }
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) {
        return missing;
    }

    return iter->second;
}

void DalyBmsController::announceStatus(DalyBmsController::Status status)
{
    if ((_lastStatus == status) && !_lastStatusPrinted.occured()) {
        return;
    }

    MessageOutput.printf("%s %s\r\n", TAG, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

void DalyBmsController::loop()
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;
    uint8_t pollInterval = cBattery.PollInterval;

    const uint32_t now = millis();
    if (_receiving && (now - _lastResponse >= 180)) {
        // last response too long ago. Reset RX index.
        MessageOutput.printf("%s Last response too long ago. Reset RX index.\r\n", TAG);
        _rxBuffer.clear();
        _receiving = false;
    }
    if ((now - _lastResponse >= 200) && !_triggerNext) {
        // last response longer than 0.2s ago -> trigger next request
        _lastResponse = now;
        _triggerNext = true;
    }
    if (now - _lastParameterReceived > 30000) { _lastParameterReceived = now; _readParameter = true; _nextRequest=0; } // every 30 seconds read parameters

    _wasActive = false;

    if (HwSerial.available()) {
        _lastResponse = now;
    }

    while (HwSerial.available()) {
        rxData(HwSerial.read());
    }

    sendRequest(pollInterval);

    if (_wasActive) MessageOutput.printf ("%s round trip time %lu ms\r\n", TAG, millis()-now);

    if (millis() - _lastRequest > 2 * pollInterval * 1000 + 250) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void DalyBmsController::sendRequest(uint8_t pollInterval)
{

    if (_nextRequest == 0) {
        if ((millis() - _lastRequest) < pollInterval * 1000) {
            return announceStatus(Status::WaitingForPollInterval);
        }
    } else {
        if ((millis() - _lastRequest) < 100) { // 100 ms
            return;
        }
    }

    if (_readParameter) {
        if (_triggerNext) {
            _triggerNext = false;
            switch (_nextRequest) {
                case 0:
                    MessageOutput.printf("%s request parameters\r\n", TAG);
                    requestData(Command::REQUEST_RATED_CAPACITY_CELL_VOLTAGE);
                    break;
                case 1:
                    requestData(Command::REQUEST_ACQUISITION_BOARD_INFO);
                    break;
                case 2:
                    requestData(Command::REQUEST_CUMULATIVE_CAPACITY);
                    break;
                case 3:
                    requestData(Command::REQUEST_BATTERY_TYPE_INFO);
                    break;
                case 4:
                    requestData(Command::REQUEST_FIRMWARE_INDEX);
                    break;
                case 5:
                    requestData(Command::REQUEST_IP);
                    break;
                case 6:
                    requestData(Command::REQUEST_BATTERY_CODE);
                    break;
                case 7:
                    requestData(Command::REQUEST_MIN_MAX_CELL_VOLTAGE);
                    break;
                case 8:
                    requestData(Command::REQUEST_MIN_MAX_PACK_VOLTAGE);
                    break;
                case 9:
                    requestData(Command::REQUEST_MAX_PACK_DISCHARGE_CHARGE_CURRENT);
                    break;
                case 10:
                    requestData(Command::REQUEST_MIN_MAX_SOC_LIMIT);
                    break;
                case 11:
                    requestData(Command::REQUEST_VOLTAGE_TEMPERATURE_DIFFERENCE);
                    break;
                case 12:
                    requestData(Command::REQUEST_BALANCE_START_DIFF_VOLTAGE);
                    break;
                case 13:
                    requestData(Command::REQUEST_SHORT_CURRENT_RESISTANCE);
                    break;
                case 14:
                    requestData(Command::REQUEST_RTC);
                    break;
                case 15:
                    requestData(Command::REQUEST_BMS_SW_VERSION);
                    break;
                case 16:
                    requestData(Command::REQUEST_BMS_HW_VERSION);
                    MessageOutput.printf("%s request parameters done\r\n", TAG);
                    _nextRequest = 0;
                    _readParameter = false;
            }
        }
        return;
    }

    if (_triggerNext) {
        _triggerNext = false;
        switch (_nextRequest) {
            case 0:
                MessageOutput.printf("%s request data\r\n", TAG);
                requestData(Command::REQUEST_BATTERY_LEVEL);
                break;
            case 1:
                requestData(Command::REQUEST_MIN_MAX_VOLTAGE);
                break;
            case 2:
                requestData(Command::REQUEST_MIN_MAX_TEMPERATURE);
                break;
            case 3:
                requestData(Command::REQUEST_MOS);
                break;
            case 4:
                requestData(Command::REQUEST_STATUS);
                break;
            case 5:
                requestData(Command::REQUEST_CELL_VOLTAGE);
                break;
            case 6:
                requestData(Command::REQUEST_TEMPERATURE);
                break;
            case 7:
                requestData(Command::REQUEST_CELL_BALANCE_STATES);
                break;
            case 8:
                requestData(Command::REQUEST_FAILURE_CODES);
                MessageOutput.printf("%s request data done\r\n", TAG);
                _nextRequest=0;
        }
    }
}

bool DalyBmsController::requestData(uint8_t data_id) // new function to request global data
{
    uint8_t request_message[DALY_FRAME_SIZE];

    request_message[0] = START_BYTE;         // Start Flag
    request_message[1] = _addr;  // Communication Module Address
    request_message[2] = data_id;      // Data ID
    request_message[3] = 0x08;         // Data Length (Fixed)
    request_message[4] = 0x00;         // Empty Data
    request_message[5] = 0x00;         //     |
    request_message[6] = 0x00;         //     |
    request_message[7] = 0x00;         //     |
    request_message[8] = 0x00;         //     |
    request_message[9] = 0x00;         //     |
    request_message[10] = 0x00;        //     |
    request_message[11] = 0x00;        // Empty Data

    request_message[12] = (uint8_t) (request_message[0] + request_message[1] + request_message[2] +
                                   request_message[3]);  // Checksum (Lower byte of the other bytes sum)

    if (Battery._verboseLogging) MessageOutput.printf("%s %d Request datapacket Nr %x\r\n", TAG, _nextRequest, data_id);

    HwSerial.write(request_message, sizeof(request_message));

    _nextRequest++;

    _lastRequest = millis();

    _wasActive = true;

    return true;
}

void DalyBmsController::rxData(uint8_t inbyte)
{
    static uint8_t _dataCount = 0;

    if (!_receiving) {
        if (inbyte != 0xa5)
            return;
        _receiving = true;
        _dataCount = 0;
    }
    _rxBuffer.push_back(inbyte);
    if (_rxBuffer.size() == 4)
        _dataCount = inbyte;
    if ((_rxBuffer.size() > 4) && (_rxBuffer.size() == _dataCount + 5)) {
        decodeData(_rxBuffer);
        _rxBuffer.clear();
        _receiving = false;
    }
}

void DalyBmsController::decodeData(std::vector<uint8_t> rxBuffer) {
    auto it = rxBuffer.begin();

    while ((it = std::find(it, rxBuffer.end(), 0xA5)) != rxBuffer.end()) {
        if (rxBuffer.end() - it >= DALY_FRAME_SIZE && it[1] == 0x01) {
            uint8_t checksum;
            int sum = 0;
            for (int i = 0; i < 12; i++) {
                sum += it[i];
            }
            checksum = sum;

            if (checksum == 0) {
                MessageOutput.printf("%s NO DATA\r\n", TAG);
                return;
            }

            if (checksum == it[12]) {
                _wasActive = true;
                if (Battery._verboseLogging) MessageOutput.printf("%s Receive datapacket Nr %02X\r\n", TAG, it[2]);
                switch (it[2]) {
                    case Command::REQUEST_RATED_CAPACITY_CELL_VOLTAGE:
                        _stats->ratedCapacity = (float)ToUint32(&it[4]) / 1000; // in Ah
                        _stats->_ratedCellVoltage = to_Volt(&it[10]); // in V
                        if (Battery._verboseLogging) MessageOutput.printf("%s rated capacity: %fV,  rated cell voltage: %fV\r\n", TAG, _stats->ratedCapacity, _stats->_ratedCellVoltage);
                        break;

                    case Command::REQUEST_ACQUISITION_BOARD_INFO:
                        _stats->_numberOfAcquisitionBoards = it[4];
                        for(uint i=0; i<3; i++) {
                            _stats->_numberOfCellsBoard[i] = it[5+i];
                            _stats->_numberOfNtcsBoard[i] = it[8+i];
                        }
                        break;

                    case Command::REQUEST_CUMULATIVE_CAPACITY:
                        _stats->_cumulativeChargeCapacity = ToUint32(&it[4]); // in Ah
                        _stats->_cumulativeDischargeCapacity = ToUint32(&it[8]); // in Ah
                        if (Battery._verboseLogging) MessageOutput.printf("%s cumulative charge capacity: %uAh,  cumulative discharge capacity: %uAh\r\n", TAG, _stats->_cumulativeChargeCapacity, _stats->_cumulativeDischargeCapacity);
                        break;

                    case Command::REQUEST_BATTERY_TYPE_INFO:
                        _stats->_batteryType = it[4];
                        snprintf (_stats->_batteryProductionDate, 31, "%u", ToUint32(&it[5]));
                        _stats->_bmsSleepTime = ToUint16(&it[9]);  // in seconds
                        _stats->_currentWave =(float)it[11] / 10.0;  // in A
                        break;

                    case Command::REQUEST_BATTERY_CODE:
                        if (it[4] > 0 && it[4] <= 4) {
                            for (uint8_t i=0; i<7; i++) {
                                uint8_t c = it[5+i];
                                _stats->_batteryCode[((it[4]-1)*7)+i] = c < 32?0:c;
                            }
                            _stats->_batteryCode[it[4]*7] = 0;
                            if (Battery._verboseLogging) MessageOutput.printf("%s %d battery code: %s\r\n", TAG, it[4], _stats->_batteryCode);
                        }
                        break;

                    case Command::REQUEST_SHORT_CURRENT_RESISTANCE:
                        _stats->_shortCurrent = ToUint16(&it[4]); // in A
                        _stats->_currentSamplingResistance = (float)ToUint16(&it[6])/1000.0; // in mOhm
                        break;

                    case Command::REQUEST_MIN_MAX_CELL_VOLTAGE:
                        _stats->AlarmValues.maxCellVoltage = to_Volt(&it[4]); // in V
                        _stats->WarningValues.maxCellVoltage = to_Volt(&it[6]); // in V
                        _stats->AlarmValues.minCellVoltage = to_Volt(&it[8]); // in V
                        _stats->WarningValues.minCellVoltage = to_Volt(&it[10]); // in V
                        if (Battery._verboseLogging) {
                            MessageOutput.printf("%s Alarm max cell voltage: %fV, min cell voltage: %fV\r\n", TAG,
                                _stats->AlarmValues.maxCellVoltage, _stats->AlarmValues.minCellVoltage);
                            MessageOutput.printf("%s Warning max cell voltage: %fV, min cell voltage: %fV\r\n", TAG,
                               _stats->WarningValues.maxCellVoltage, _stats->WarningValues.minCellVoltage);
                        }
                        break;

                    case Command::REQUEST_MIN_MAX_PACK_VOLTAGE:
                        _stats->AlarmValues.maxPackVoltage = (float)ToUint16(&it[4]) / 10; // in V
                        _stats->WarningValues.maxPackVoltage = (float)ToUint16(&it[6]) / 10; // in V
                        _stats->AlarmValues.minPackVoltage = (float)ToUint16(&it[8]) / 10.0; // in V
                        _stats->WarningValues.minPackVoltage = (float)ToUint16(&it[10]) / 10.0; // in V
                        if (Battery._verboseLogging) {
                            MessageOutput.printf("%s Alarm max pack voltage: %fV, min pack voltage: %fV\r\n", TAG,
                                _stats->AlarmValues.maxPackVoltage, _stats->AlarmValues.minPackVoltage);
                            MessageOutput.printf("%s Warning max pack voltage: %fV, min pack voltage: %fV\r\n", TAG,
                                _stats->WarningValues.maxPackVoltage, _stats->WarningValues.minPackVoltage);
                        }
                        break;

                    case Command::REQUEST_MAX_PACK_DISCHARGE_CHARGE_CURRENT:
                        _stats->AlarmValues.maxPackChargeCurrent = (DALY_CURRENT_OFFSET - (float)ToUint16(&it[4])) / 10.0; // in A
                        _stats->WarningValues.maxPackChargeCurrent = (DALY_CURRENT_OFFSET - (float)ToUint16(&it[6])) / 10.0; // in A
                        _stats->AlarmValues.maxPackDischargeCurrent = (float)(DALY_CURRENT_OFFSET - ToUint16(&it[8])) / 10.0; // in A
                        _stats->WarningValues.maxPackDischargeCurrent = (float)(DALY_CURRENT_OFFSET - ToUint16(&it[10])) / 10.0; // in A
                        if (Battery._verboseLogging)  {
                            MessageOutput.printf("%s Alarm max pack charge current: %fA, max pack discharge current: %fA\r\n", TAG,
                                _stats->AlarmValues.maxPackChargeCurrent, _stats->AlarmValues.maxPackDischargeCurrent);
                            MessageOutput.printf("%s Warning max pack charge current: %fA, max pack discharge current: %fA\r\n", TAG,
                                _stats->WarningValues.maxPackChargeCurrent, _stats->WarningValues.maxPackDischargeCurrent);
                        }
                        break;

                    case Command::REQUEST_MIN_MAX_SOC_LIMIT:
                        _stats->WarningValues.maxSoc = (float)ToUint16(&it[4]) / 10.0; // in %
                        _stats->AlarmValues.maxSoc = (float)ToUint16(&it[6]) / 10.0; // in %
                        _stats->WarningValues.minSoc = (float)ToUint16(&it[8]) / 10.0; // in %
                        _stats->AlarmValues.minSoc = (float)ToUint16(&it[10]) / 10.0; // in %
                        if (Battery._verboseLogging) {
                            MessageOutput.printf("%s Alarm max SoC: %f%%, min SoC: %f%%\r\n", TAG, _stats->AlarmValues.maxSoc, _stats->AlarmValues.minSoc);
                            MessageOutput.printf("%s Warning max SoC: %f%%, min SoC: %f%%\r\n", TAG, _stats->WarningValues.maxSoc, _stats->WarningValues.minSoc);
                        }
                        break;

                    case Command::REQUEST_VOLTAGE_TEMPERATURE_DIFFERENCE:
                        _stats->WarningValues.cellVoltageDifference = (float)ToUint16(&it[4]); // in mV
                        _stats->AlarmValues.cellVoltageDifference = (float)ToUint16(&it[6]); // in mV
                        _stats->WarningValues.temperatureDifference = it[8]; // in in°C
                        _stats->AlarmValues.temperatureDifference = it[9]; // in °C
                        break;

                    case Command::REQUEST_BALANCE_START_DIFF_VOLTAGE:
                        _stats->_balanceStartVoltage = to_Volt(&it[4]); // in V
                        _stats->_balanceDifferenceVoltage = to_Volt(&it[6]); // in V
                        break;

                    case Command::REQUEST_BMS_SW_VERSION:
                        if (it[4] > 0 && it[4] <= 3) {
                            for (uint8_t i=0; i<7; i++) _stats->_bmsSWversion[((it[4]-1)*7)+i] = (it[5+i]);
                            _stats->_bmsSWversion[it[4]*7] = 0;
                        }
                        if (Battery._verboseLogging) MessageOutput.printf("%s %d bms SW version: %s\r\n", TAG, it[4], _stats->_bmsSWversion);
                        break;

                    case Command::REQUEST_BMS_HW_VERSION:
                        if (it[4] > 0 && it[4] <= 3) {
                            for (uint8_t i=0; i<7; i++) _stats->_bmsHWversion[((it[4]-1)*7)+i] = (it[5+i]);
                            _stats->_bmsHWversion[it[4]*7] = 0;
                        }
                        if (Battery._verboseLogging) MessageOutput.printf("%s %d bms HW version: %s\r\n", TAG, it[4], _stats->_bmsHWversion);
                        _stats->_manufacturer = _stats->_bmsHWversion;
                        break;

                    case Command::REQUEST_BATTERY_LEVEL:
                        _stats->voltage = (float) ToUint16(&it[4]) / 10.0;
                        _stats->current = (float) (ToUint16(&it[8]) - DALY_CURRENT_OFFSET) / 10.0;
                        _stats->power = _stats->current * _stats->voltage / 1000.0;
                        _stats->batteryLevel = (float) ToUint16(&it[10]) / 10.0;
                        _stats->setSoC(static_cast<uint8_t>((100.0 * _stats->remainingCapacity / _stats->ratedCapacity) + 0.5));
                        _stats->chargeImmediately = _stats->batteryLevel < _stats->AlarmValues.minSoc;
                        if (Battery._verboseLogging)  {
                            MessageOutput.printf("%s voltage: %fV, current: %fA\r\n", TAG, _stats->voltage, _stats->current);
                            MessageOutput.printf("%s battery level: %f%%\r\n", TAG, _stats->batteryLevel);
                        }
                        break;

                    case Command::REQUEST_MIN_MAX_VOLTAGE:
                        _stats->maxCellVoltage = to_Volt(&it[4]);
                        _stats->_maxCellVoltageNumber = it[6];
                        _stats->minCellVoltage = to_Volt(&it[7]);
                        _stats->_minCellVoltageNumber = it[9];
                        _stats->cellDiffVoltage = (_stats->maxCellVoltage - _stats->minCellVoltage) * 1000.0;
                        break;

                    case Command::REQUEST_MIN_MAX_TEMPERATURE:
                        _stats->_maxTemperature = (it[4] - DALY_TEMPERATURE_OFFSET);
                        _stats->_maxTemperatureProbeNumber = it[5];
                        _stats->_minTemperature = (it[6] - DALY_TEMPERATURE_OFFSET);
                        _stats->_minTemperatureProbeNumber = it[7];
                        _stats->averageBMSTemperature = (_stats->_maxTemperature + _stats->_minTemperature) / 2;
                        break;
                    case Command::REQUEST_MOS:
                          switch (it[4]) {
                            case 0:
                                strcpy(_stats->_status, "Stationary");
                                break;
                            case 1:
                                strcpy(_stats->_status, "Charging");
                                break;
                            case 2:
                                strcpy(_stats->_status, "Discharging");
                                break;
                            default:
                                break;
                        }
                        _stats->chargingMosEnabled  = it[5];
                        _stats->dischargingMosEnabled = it[6];
                        _stats->_bmsCycles = it[7];
                        _stats->remainingCapacity = (float) ToUint32(&it[8]) / 1000;
                        if (Battery._verboseLogging)  {
                            MessageOutput.printf("%s status: %s, bms cycles: %d\r\n", TAG, _stats->_status, _stats->_bmsCycles);
                            MessageOutput.printf("%s remaining capacity: %fAh\r\n", TAG, _stats->remainingCapacity);
                        }
                        break;

                    case Command::REQUEST_STATUS:
                        _stats->_cellsNumber = it[4];
                        _stats->_tempsNumber = it[5];
                        _stats->_chargeState = it[6];
                        _stats->_loadState = it[7];
                        _stats->_dio[8] = 0;
                        for (uint8_t i=0; i<8; i++) _stats->_dio[i] = (it[8] & (128>>i))?'1':'0';
                        _stats->batteryCycles = ToUint16(&it[9]);
                        if (Battery._verboseLogging) MessageOutput.printf("%s charge state %d, load state %d, dio %s, battery cycles %u\r\n", TAG, _stats->_chargeState, _stats->_loadState, _stats->_dio, _stats->batteryCycles);
                        break;

                    case Command::REQUEST_TEMPERATURE:
                        if (it[4] > 0 && it[4] < 4) {
                            for (uint8_t i=0; i<7; i++)
                                _stats->_temperature[(it[4]-1)*3+i] = (it[5+i] - DALY_TEMPERATURE_OFFSET);
                        }
                        if (Battery._verboseLogging) MessageOutput.printf("%s it[4] %d\r\n", TAG, it[4]);
                        break;

                    case Command::REQUEST_CELL_VOLTAGE:
                        if (it[4] > 0 && it[4] < 9) {
                            for (uint8_t i=0; i<3; i++) {
                                _stats->_cellVoltage[(it[4]-1)*3+i] = to_Volt(&it[5+i*2]);
                            }
                        }
                        if (Battery._verboseLogging) MessageOutput.printf("%s it[4] %d\r\n", TAG, it[4]);
                        break;

                    case Command::REQUEST_CELL_BALANCE_STATES:
                        for(uint8_t j=0; j<3; j++) {
                            for (uint8_t i=0; i<8; i++) _stats->_cellBalance[j*(8+1)+i] = (it[4+j] & (128>>i))?'1':'0';
                            _stats->_cellBalance[j*(8+1)+8] = ' ';
                        }
                        _stats->_cellBalance[(8+1)*2+8] = 0;
                        if (Battery._verboseLogging) MessageOutput.printf("%s cell balance %s\r\n", TAG, _stats->_cellBalance);
                        break;

                    case Command::REQUEST_FAILURE_CODES:
                        {
                            char bytes[72];
                            for(uint8_t j=0; j<7; j++) {
                                _stats->FailureStatus.bytes[j] = it[4+j];
                                for (uint8_t i=0; i<8; i++) bytes[j*(8+1)+i] = (it[4+j] & (128>>i))?'1':'0';
                                bytes[j*(8+1)+8] = ' ';
                            }
                            bytes[(8+1)*6+8] = 0;
                            _stats->_faultCode = it[11];
                            if (Battery._verboseLogging) MessageOutput.printf("%s failure status %s, fault code: %02X\r\n", TAG, bytes, _stats->_faultCode);
                        }
                        _stats->Warning.lowVoltage = _stats->FailureStatus.levelOneCellVoltageTooLow ||
                                                    _stats->FailureStatus.levelOnePackVoltageTooLow;
                        _stats->Warning.highVoltage = _stats->FailureStatus.levelOneCellVoltageTooHigh ||
                                                    _stats->FailureStatus.levelOnePackVoltageTooHigh;
                        _stats->Warning.lowTemperature = _stats->FailureStatus.levelOneChargeTempTooLow ||
                                                        _stats->FailureStatus.levelOneDischargeTempTooLow;
                        _stats->Warning.highTemperature =  _stats->FailureStatus.levelOneChargeTempTooHigh ||
                                                        _stats->FailureStatus.levelOneDischargeTempTooHigh;
                        _stats->Warning.highCurrentCharge = _stats->FailureStatus.levelOneChargeCurrentTooHigh;
                        _stats->Warning.highCurrentDischarge = _stats->FailureStatus.levelOneDischargeCurrentTooHigh;
                        _stats->Warning.cellImbalance = _stats->FailureStatus.levelOneCellVoltageDifferenceTooHigh;

                        _stats->Alarm.underVoltage = _stats->FailureStatus.levelTwoCellVoltageTooLow ||
                                                    _stats->FailureStatus.levelTwoPackVoltageTooLow;
                        _stats->Alarm.overVoltage = _stats->FailureStatus.levelTwoCellVoltageTooHigh ||
                                                    _stats->FailureStatus.levelTwoPackVoltageTooHigh;
                        _stats->Alarm.underTemperature = _stats->FailureStatus.levelTwoChargeTempTooLow ||
                                                        _stats->FailureStatus.levelTwoDischargeTempTooLow;
                        _stats->Alarm.overTemperature =  _stats->FailureStatus.levelTwoChargeTempTooHigh ||
                                                        _stats->FailureStatus.levelTwoDischargeTempTooHigh ||
                                                        _stats->FailureStatus.chargeFETTemperatureTooHigh ||
                                                        _stats->FailureStatus.dischargeFETTemperatureTooHigh;
                        _stats->Alarm.overCurrentCharge = _stats->FailureStatus.levelTwoChargeCurrentTooHigh;
                        _stats->Alarm.overCurrentDischarge = _stats->FailureStatus.levelTwoDischargeCurrentTooHigh;
                        _stats->Alarm.bmsInternal = _stats->FailureStatus.failureOfChargeFETTemperatureSensor ||
                                                    _stats->FailureStatus.failureOfDischargeFETTemperatureSensor ||
                                                    _stats->FailureStatus.failureOfMainVoltageSensorModule ||
                                                    _stats->FailureStatus.failureOfChargeFETBreaker ||
                                                    _stats->FailureStatus.failureOfDischargeFETBreaker ||
                                                    _stats->FailureStatus.failureOfEEPROMStorageModule ||
                                                    _stats->FailureStatus.failureOfPrechargeModule ||
                                                    _stats->FailureStatus.failureOfRealtimeClockModule ||
                                                    _stats->FailureStatus.failureOfShortCircuitProtection ||
                                                    _stats->FailureStatus.failureOfTemperatureSensorModule ||
                                                    _stats->FailureStatus.failureOfVoltageSensorModule;
                        _stats->Alarm.cellImbalance = _stats->FailureStatus.levelTwoCellVoltageDifferenceTooHigh;
                        break;
                    case Command::WRITE_DISCHRG_FET:
                        break;
                    case Command::WRITE_CHRG_FET:
                        break;
                    case Command::WRITE_BMS_RESET:
                        break;
                    default:
                        _wasActive = false;
                }
                _stats->setLastUpdate(millis());
            } else {
                MessageOutput.printf("%s Checksum-Error on Packet %x\r\n", TAG, it[4]);
            }
            std::advance(it, DALY_FRAME_SIZE);
        } else {
            std::advance(it, 1);
            if (it[1] >= 0x20) {
                MessageOutput.printf("%s BMS SLEEPING\r\n", TAG);
            }
        }
    }
}

void DalyBmsController::reset()
{
    _rxBuffer.clear();
    return;
}

void DalyBmsController::clearGet(void)
{

    // data from 0x90
    // get.packVoltage = 0; // pressure (0.1 V)
    // get.packCurrent = 0; // acquisition (0.1 V)
    // get.packSOC = 0;     // State Of Charge

    // data from 0x91
    // get.maxCellmV = 0;   // maximum monomer voltage (mV)
    // get.maxCellVNum = 0; // Maximum Unit Voltage cell No.
    // get.minCellmV = 0;   // minimum monomer voltage (mV)
    // get.minCellVNum = 0; // Minimum Unit Voltage cell No.
    // get.cellDiff = 0;    // difference betwen cells

    // data from 0x92
    // get.tempAverage = 0; // Avergae Temperature

    // data from 0x93
    strcpy(_stats->_status, "offline"); // charge/discharge status (0 stationary ,1 charge ,2 discharge)

    // get.chargeFetState = false;    // charging MOS tube status
    // get.disChargeFetState = false; // discharge MOS tube state
    // get.bmsHeartBeat = 0;          // BMS life(0~255 cycles)
    // get.resCapacitymAh = 0;        // residual capacity mAH

    // data from 0x94
    // get.numberOfCells = 0;                   // amount of cells
    // get.numOfTempSensors = 0;                // amount of temp sensors
    // get.chargeState = 0;                     // charger status 0=disconnected 1=connected
    // get.loadState = 0;                       // Load Status 0=disconnected 1=connected
    // memset(get.dIO, false, sizeof(get.dIO)); // No information about this
    // get.bmsCycles = 0;                       // charge / discharge cycles

    // data from 0x95
    // memset(get.cellVmV, 0, sizeof(get.cellVmV)); // Store Cell Voltages in mV

    // data from 0x96
    // memset(get.cellTemperature, 0, sizeof(get.cellTemperature)); // array of cell Temperature sensors

    // data from 0x97
    // memset(get.cellBalanceState, false, sizeof(get.cellBalanceState)); // bool array of cell balance states
    // get.cellBalanceActive = false;                                     // bool is cell balance active
}

//#endif
