#ifdef USE_DALYBMS_CONTROLLER

#include "DalyBmsController.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PinMapping.h"
#include "SerialPortManager.h"
#include <Arduino.h>
#include <driver/uart.h>
#include <ctime>
#include <frozen/map.h>

static constexpr char TAG[] = "[Daly BMS]";

bool DalyBmsController::init()
{
    MessageOutput.printf("Initialize Daly BMS Controller... ");

    _lastStatusPrinted.set(10 * 1000);

    const PinMapping_t& pin = PinMapping.get();

    if (pin.battery_rx < 0 || pin.battery_tx < 0 || (pin.battery_rx == pin.battery_tx) || (pin.battery_rts >= 0 && (pin.battery_rts == pin.battery_rx || pin.battery_rts == pin.battery_tx))) {
        MessageOutput.println("Invalid TX/RX pin config");
        return false;
    }

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    _upSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);

    _upSerial->begin(9600, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    if (pin.battery_rts >= -1) {
        /*
         * Daly BMS is connected via a RS485 module. Two different types of modules are supported.
         * Type 1: If a GPIO pin greater 0 is given, we have a MAX3485 or SP3485 module with external driven DE/RE pins
         *         Both pins are connected together and will be driven by the HWSerial driver.
         * Type 2: If the GPIO is -1, we assume that we have a RS485 TTL module with a self controlled DE/RE circuit.
         *         In this case we only need a TX and RX pin.
         */
        MessageOutput.printf("RS485 module (Type %d) rx = %d, tx = %d", pin.battery_rts >= 0 ? 1 : 2, pin.battery_rx, pin.battery_tx);
        if (pin.battery_rts >= 0) {
            MessageOutput.printf(", rts = %d", pin.battery_rts);
            _upSerial->setPins(pin.battery_rx, pin.battery_tx, UART_PIN_NO_CHANGE, pin.battery_rts);
        }

        // RS485 protocol is half duplex
        ESP_ERROR_CHECK(uart_set_mode(*oHwSerialPort, UART_MODE_RS485_HALF_DUPLEX));

    } else {
        // pin.battery_rts is negativ and less -1, Daly BMS is connected via RS232
        MessageOutput.printf("RS232 module rx = %d, tx = %d", pin.battery_rx, pin.battery_tx);
    }
    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(*oHwSerialPort, ECHO_READ_TOUT));

    _upSerial->flush();

    while (_upSerial->available()) { // clear serial read buffer
        _upSerial->read();
        vTaskDelay(1);
    }

    memset(_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    clearGet();

    if (PinMapping.get().battery_wakeup >= 0) {
        pinMode(PinMapping.get().battery_wakeup, OUTPUT);
        digitalWrite(PinMapping.get().battery_wakeup, HIGH);
        vTaskDelay(500);
        digitalWrite(PinMapping.get().battery_wakeup, LOW);
    }

    MessageOutput.print(" initialized successfully.");

    _initialized = true;

    return true;
}

void DalyBmsController::deinit()
{
    _upSerial->end();

    if (PinMapping.get().battery_rts >= 0) { pinMode(PinMapping.get().battery_rts, INPUT); }

    SerialPortManager.freePort(_serialPortOwner);

    _initialized = false;

    MessageOutput.printf("%s Serial driver uninstalled\r\n", TAG);
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
    if ((now - _lastResponse >= 150) && !_triggerNext) {
        // last response longer than 0.15s ago -> trigger next request
        _lastResponse = now;
        _triggerNext = true;
    }
    if (now - _lastParameterReceived > 30000) {
        _lastParameterReceived = now;
        _readParameter = true;
        _nextRequest = 0;
    } // every 30 seconds read parameters

    _wasActive = false;

    if (_upSerial->available()) {
        _lastResponse = now;
    }

    while (_upSerial->available()) {
        rxData(_upSerial->read());
    }

    sendRequest(pollInterval);

    //    if (_wasActive) MessageOutput.printf ("%s round trip time %lu ms\r\n", TAG, millis()-now);

    if (millis() - _lastRequest > 2 * pollInterval * 1000 + 250) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void DalyBmsController::sendRequest(uint8_t pollInterval)
{

    if (_nextRequest == 0) {
        if ((millis() - _lastRequest) < pollInterval * 1000) {
            if (PinMapping.get().battery_wakeup >= 0) {
                if ((millis() - _lastRequest) > 50 && (millis() - _lastRequest) < 600) { // 500ms High Impuls
                    digitalWrite(PinMapping.get().battery_wakeup, HIGH);
                } else {
                    digitalWrite(PinMapping.get().battery_wakeup, LOW);
                }
            }
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
            static constexpr uint8_t ParameterRequests[] = {
                Command::REQUEST_RATED_CAPACITY_CELL_VOLTAGE,
                Command::REQUEST_ACQUISITION_BOARD_INFO,
                Command::REQUEST_CUMULATIVE_CAPACITY,
                Command::REQUEST_BATTERY_TYPE_INFO,
                Command::REQUEST_FIRMWARE_INDEX,
                Command::REQUEST_IP,
                Command::REQUEST_BATTERY_CODE,
                Command::REQUEST_MIN_MAX_CELL_VOLTAGE,
                Command::REQUEST_MIN_MAX_PACK_VOLTAGE,
                Command::REQUEST_MAX_PACK_DISCHARGE_CHARGE_CURRENT,
                Command::REQUEST_MIN_MAX_SOC_LIMIT,
                Command::REQUEST_VOLTAGE_TEMPERATURE_DIFFERENCE,
                Command::REQUEST_BALANCE_START_DIFF_VOLTAGE,
                Command::REQUEST_SHORT_CURRENT_RESISTANCE,
                Command::REQUEST_RTC,
                Command::REQUEST_BMS_SW_VERSION,
                Command::REQUEST_BMS_HW_VERSION
            };
            if (_nextRequest == 0) MessageOutput.printf("%s request parameters\r\n", TAG);
            requestData(ParameterRequests[_nextRequest]);
            if (_nextRequest == sizeof(ParameterRequests)/sizeof(uint8_t)-1) {
                MessageOutput.printf("%s request parameters done\r\n", TAG);
                _nextRequest = 0;
                _readParameter = false;
            }
        }
        return;
    }

    if (_triggerNext) {
        _triggerNext = false;
        static constexpr uint8_t Requests[] = {
            Command::REQUEST_BATTERY_LEVEL,
            Command::REQUEST_MIN_MAX_VOLTAGE,
            Command::REQUEST_MIN_MAX_TEMPERATURE,
            Command::REQUEST_MOS,
            Command::REQUEST_STATUS,
            Command::REQUEST_CELL_VOLTAGE,
            Command::REQUEST_TEMPERATURE,
            Command::REQUEST_CELL_BALANCE_STATES,
            Command::REQUEST_FAILURE_CODES
        };
        if (_nextRequest == 0) MessageOutput.printf("%s request data\r\n", TAG);
        requestData(Requests[_nextRequest]);
        if (_nextRequest == sizeof(Requests)/sizeof(uint8_t)-1) {
            MessageOutput.printf("%s request data done\r\n", TAG);
            _nextRequest = 0;
        }
    }
}

bool DalyBmsController::requestData(uint8_t data_id) // new function to request global data
{
    uint8_t request_message[DALY_FRAME_SIZE];

    request_message[0] = START_BYTE; // Start Flag
    request_message[1] = _addr; // Communication Module Address
    request_message[2] = data_id; // Data ID
    request_message[3] = 0x08; // Data Length (Fixed)
    request_message[4] = 0x00; // Empty Data
    request_message[5] = 0x00; //     |
    request_message[6] = 0x00; //     |
    request_message[7] = 0x00; //     |
    request_message[8] = 0x00; //     |
    request_message[9] = 0x00; //     |
    request_message[10] = 0x00; //     |
    request_message[11] = 0x00; // Empty Data

    request_message[12] = (uint8_t)(request_message[0] + request_message[1] + request_message[2] + request_message[3]); // Checksum (Lower byte of the other bytes sum)

    if (_verboseLogging)
        MessageOutput.printf("%s %d Request datapacket Nr %x\r\n", TAG, _nextRequest, data_id);

    _upSerial->write(request_message, sizeof(request_message));

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

void DalyBmsController::decodeData(std::vector<uint8_t> rxBuffer)
{
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
                if (_verboseLogging)
                    MessageOutput.printf("%s Receive datapacket Nr %02X\r\n", TAG, it[2]);
                switch (it[2]) {
                case Command::REQUEST_RATED_CAPACITY_CELL_VOLTAGE:
                    _stats->ratedCapacity = to_AmpHour(&it[4]); // in Ah 0.001
                    _stats->_ratedCellVoltage = to_Volt_001(&it[10]); // in V 0.001
                    if (_verboseLogging)
                        MessageOutput.printf("%s rated capacity: %.3fAh,  rated cell voltage: %.3fV\r\n", TAG, _stats->ratedCapacity, _stats->_ratedCellVoltage);
                    break;

                case Command::REQUEST_ACQUISITION_BOARD_INFO:
                    _stats->_numberOfAcquisitionBoards = it[4];
                    for (uint i = 0; i < 3; i++) {
                        _stats->_numberOfCellsBoard[i] = it[5 + i];
                        _stats->_numberOfNtcsBoard[i] = it[8 + i];
                    }
                    break;

                case Command::REQUEST_CUMULATIVE_CAPACITY:
                    _stats->_cumulativeChargeCapacity = ToUint32(&it[4]); // in Ah
                    _stats->_cumulativeDischargeCapacity = ToUint32(&it[8]); // in Ah
                    if (_verboseLogging)
                        MessageOutput.printf("%s cumulative charge capacity: %uAh,  cumulative discharge capacity: %uAh\r\n", TAG, _stats->_cumulativeChargeCapacity, _stats->_cumulativeDischargeCapacity);
                    break;

                case Command::REQUEST_BATTERY_TYPE_INFO:
                    _stats->_batteryType = it[4];
                    snprintf(_stats->_batteryProductionDate, sizeof(_stats->_batteryProductionDate), "%u", ToUint32(&it[5]));
                    _stats->_bmsSleepTime = ToUint16(&it[9]); // in seconds
                    _stats->_currentWave = static_cast<float>(it[11]) / 10.0; // in A
                    break;

                case Command::REQUEST_BATTERY_CODE:
                    if (it[4] > 0 && it[4] <= 4) {
                        for (uint8_t i = 0; i < 7; i++) {
                            uint8_t c = it[5 + i];
                            _stats->_batteryCode[((it[4] - 1) * 7) + i] = c < 32 ? 0 : c;
                        }
                        _stats->_batteryCode[it[4] * 7] = 0;
                        if (_verboseLogging)
                            MessageOutput.printf("%s %d battery code: %s\r\n", TAG, it[4], _stats->_batteryCode);
                    }
                    break;

                case Command::REQUEST_SHORT_CURRENT_RESISTANCE:
                    _stats->_shortCurrent = ToUint16(&it[4]); // in A
                    _stats->currentSamplingResistance = to_mOhm(&it[6]); // in mOhm
                    break;

                case Command::REQUEST_MIN_MAX_CELL_VOLTAGE:
                    _stats->AlarmValues.maxCellVoltage = to_Volt_001(&it[4]); // in V
                    _stats->WarningValues.maxCellVoltage = to_Volt_001(&it[6]); // in V
                    _stats->AlarmValues.minCellVoltage = to_Volt_001(&it[8]); // in V
                    _stats->WarningValues.minCellVoltage = to_Volt_001(&it[10]); // in V
                    if (_verboseLogging) {
                        MessageOutput.printf("%s Alarm max cell voltage: %.3fV, min cell voltage: %.3fV\r\n", TAG,
                            _stats->AlarmValues.maxCellVoltage, _stats->AlarmValues.minCellVoltage);
                        MessageOutput.printf("%s Warning max cell voltage: %.3fV, min cell voltage: %.3fV\r\n", TAG,
                            _stats->WarningValues.maxCellVoltage, _stats->WarningValues.minCellVoltage);
                    }
                    break;

                case Command::REQUEST_MIN_MAX_PACK_VOLTAGE:
                    _stats->AlarmValues.maxPackVoltage = static_cast<float>(ToUint16(&it[4])) / 10.0f; // in V
                    _stats->WarningValues.maxPackVoltage = static_cast<float>(ToUint16(&it[6])) / 10.0f; // in V
                    _stats->AlarmValues.minPackVoltage = static_cast<float>(ToUint16(&it[8])) / 10.0f; // in V
                    _stats->WarningValues.minPackVoltage = static_cast<float>(ToUint16(&it[10])) / 10.0f; // in V
                    if (_verboseLogging) {
                        MessageOutput.printf("%s Alarm max pack voltage: %.1fV, min pack voltage: %.1fV\r\n", TAG,
                            _stats->AlarmValues.maxPackVoltage, _stats->AlarmValues.minPackVoltage);
                        MessageOutput.printf("%s Warning max pack voltage: %.1fV, min pack voltage: %.1fV\r\n", TAG,
                            _stats->WarningValues.maxPackVoltage, _stats->WarningValues.minPackVoltage);
                    }
                    break;

                case Command::REQUEST_MAX_PACK_DISCHARGE_CHARGE_CURRENT:
                    _stats->AlarmValues.maxPackChargeCurrent = to_Amp_1(&it[4]); // in A
                    _stats->WarningValues.maxPackChargeCurrent = to_Amp_1(&it[6]); // in A
                    _stats->AlarmValues.maxPackDischargeCurrent = to_Amp_1(&it[8]); // in A
                    _stats->WarningValues.maxPackDischargeCurrent = to_Amp_1(&it[10]); // in A
                    if (_verboseLogging) {
                        MessageOutput.printf("%s Alarm max pack charge current: %.1fA, max pack discharge current: %.1fA\r\n", TAG,
                            _stats->AlarmValues.maxPackChargeCurrent, _stats->AlarmValues.maxPackDischargeCurrent);
                        MessageOutput.printf("%s Warning max pack charge current: %.1fA, max pack discharge current: %.1fA\r\n", TAG,
                            _stats->WarningValues.maxPackChargeCurrent, _stats->WarningValues.maxPackDischargeCurrent);
                    }
                    break;

                case Command::REQUEST_MIN_MAX_SOC_LIMIT:
                    _stats->WarningValues.maxSoc = static_cast<float>(ToUint16(&it[4])) / 10.0f; // in %
                    _stats->AlarmValues.maxSoc = static_cast<float>(ToUint16(&it[6])) / 10.0f; // in %
                    _stats->WarningValues.minSoc = static_cast<float>(ToUint16(&it[8])) / 10.0f; // in %
                    _stats->AlarmValues.minSoc = static_cast<float>(ToUint16(&it[10])) / 10.0f; // in %
                    if (_verboseLogging) {
                        MessageOutput.printf("%s Alarm max SoC: %.1f%%, min SoC: %.1f%%\r\n", TAG, _stats->AlarmValues.maxSoc, _stats->AlarmValues.minSoc);
                        MessageOutput.printf("%s Warning max SoC: %.1f%%, min SoC: %.1f%%\r\n", TAG, _stats->WarningValues.maxSoc, _stats->WarningValues.minSoc);
                    }
                    break;

                case Command::REQUEST_VOLTAGE_TEMPERATURE_DIFFERENCE:
                    _stats->WarningValues.cellVoltageDifference = static_cast<float>(ToUint16(&it[4])); // in mV
                    _stats->AlarmValues.cellVoltageDifference = static_cast<float>(ToUint16(&it[6])); // in mV
                    _stats->WarningValues.temperatureDifference = it[8]; // in in°C
                    _stats->AlarmValues.temperatureDifference = it[9]; // in °C
                    break;

                case Command::REQUEST_BALANCE_START_DIFF_VOLTAGE:
                    _stats->_balanceStartVoltage = to_Volt_001(&it[4]); // in V
                    _stats->_balanceDifferenceVoltage = to_Volt_001(&it[6]); // in V
                    break;

                case Command::REQUEST_BMS_SW_VERSION:
                    if (it[4] > 0 && it[4] <= 3) {
                        for (uint8_t i = 0; i < 7; i++)
                            _stats->_bmsSWversion[((it[4] - 1) * 7) + i] = (it[5 + i]);
                        _stats->_bmsSWversion[it[4] * 7] = 0;
                    }
                    if (_verboseLogging)
                        MessageOutput.printf("%s %d bms SW version: %s\r\n", TAG, it[4], _stats->_bmsSWversion);
                    break;

                case Command::REQUEST_BMS_HW_VERSION:
                    if (it[4] > 0 && it[4] <= 3) {
                        for (uint8_t i = 0; i < 7; i++)
                            _stats->_bmsHWversion[((it[4] - 1) * 7) + i] = (it[5 + i]);
                        _stats->_bmsHWversion[it[4] * 7] = 0;
                    }
                    if (_verboseLogging)
                        MessageOutput.printf("%s %d bms HW version: %s\r\n", TAG, it[4], _stats->_bmsHWversion);
                    _stats->_manufacturer = String("Daly ") + String(_stats->_bmsHWversion);
                    break;

                case Command::REQUEST_BATTERY_LEVEL:
                    _stats->setVoltage(to_Volt_1(&it[4]), millis()); // in Volt 0.1
                    _stats->gatherVoltage = to_Volt_1(&it[6]); // in Volt 0.1
                    _stats->setCurrent(-to_Amp_1(&it[8]), 1/*precision*/, millis()); // in Amp 0.1
                    _stats->power = _stats->current * _stats->getVoltage() / 1000.0; // in kW
                    _stats->batteryLevel = ToFloat(&it[10]) / 10.0;
                    _stats->setSoC((100.0 * _stats->remainingCapacity) / _stats->ratedCapacity, 1 /*precision*/, millis());
                    _stats->chargeImmediately2 = _stats->batteryLevel < max(_stats->WarningValues.minSoc, static_cast<float>(9.0));
                    _stats->chargeImmediately1 = _stats->batteryLevel < max(_stats->AlarmValues.minSoc, static_cast<float>(5.0));
                    if (_verboseLogging) {
                        MessageOutput.printf("%s voltage: %.1fV, current: %.1fA\r\n", TAG, _stats->getVoltage(), _stats->current);
                        MessageOutput.printf("%s battery level: %.1f%%\r\n", TAG, _stats->batteryLevel);
                    }
                    break;

                case Command::REQUEST_MIN_MAX_VOLTAGE:
                    _stats->maxCellVoltage = to_Volt_001(&it[4]);
                    _stats->_maxCellVoltageNumber = it[6];
                    _stats->minCellVoltage = to_Volt_001(&it[7]);
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
                        strncpy(_stats->_status, "Stationary", sizeof(_stats->_status));
                        break;
                    case 1:
                        strncpy(_stats->_status, "Charging", sizeof(_stats->_status));
                        break;
                    case 2:
                        strncpy(_stats->_status, "Discharging", sizeof(_stats->_status));
                        break;
                    default:
                        break;
                    }
                    _stats->chargingMosEnabled = it[5];
                    _stats->dischargingMosEnabled = it[6];
                    _stats->_bmsCycles = it[7];
                    _stats->remainingCapacity = static_cast<float>(ToUint32(&it[8])) / 1000.0f;
                    if (_verboseLogging) {
                        MessageOutput.printf("%s status: %s, bms cycles: %d\r\n", TAG, _stats->_status, _stats->_bmsCycles);
                        MessageOutput.printf("%s remaining capacity: %.3fAh\r\n", TAG, _stats->remainingCapacity);
                    }
                    break;

                case Command::REQUEST_STATUS:
                    _stats->_cellsNumber = it[4];
                    _stats->_tempsNumber = it[5];
                    _stats->_chargeState = it[6];
                    _stats->_loadState = it[7];
                    _stats->_dIO = it[8];
                    _stats->batteryCycles = ToUint16(&it[9]);
                    if (_verboseLogging) {
                        char dIO[9];
                        dIO[8] = 0;
                        for (int i = 0; i < 8; i++)
                            dIO[i] = (_stats->_dIO & (0x80 >> i)) ? '1' : '0';
                        MessageOutput.printf("%s charge state %d, load state %d, dio %s, battery cycles %u\r\n", TAG, _stats->_chargeState, _stats->_loadState, dIO, _stats->batteryCycles);
                    }
                    break;

                case Command::REQUEST_TEMPERATURE:
                    if (_stats->_tempsNumber < DALY_MIN_NUMBER_TEMP_SENSORS || _stats->_tempsNumber > DALY_MAX_NUMBER_TEMP_SENSORS) {
                        break;
                    }

                    _stats->_temperature = reinterpret_cast<int*>(realloc(_stats->_temperature, _stats->_tempsNumber * sizeof(int)));
                    if (it[4] > 0 && it[4] < 4) {
                        for (uint8_t i = 0; i < 7; i++) {
                            int idx = (it[4] - 1) * 3 + i;
                            if (idx >= _stats->_tempsNumber)
                                break;
                            _stats->_temperature[idx] = (it[5 + i] - DALY_TEMPERATURE_OFFSET);
                        }
                    }
                    if (_verboseLogging)
                        MessageOutput.printf("%s it[4] %d\r\n", TAG, it[4]);
                    break;

                case Command::REQUEST_CELL_VOLTAGE:
                    if (_stats->_cellsNumber < DALY_MIN_NUMBER_CELLS || _stats->_cellsNumber > DALY_MAX_NUMBER_CELLS) {
                        break;
                    }

                    _stats->_cellVoltage = reinterpret_cast<float*>(realloc(_stats->_cellVoltage, _stats->_cellsNumber * sizeof(float)));
                    if (it[4] > 0 && it[4] < 9) {
                        for (uint8_t i = 0; i < 3; i++) {
                            int idx = (it[4] - 1) * 3 + i;
                            if (idx >= _stats->_cellsNumber)
                                break;
                            _stats->_cellVoltage[idx] = to_Volt_001(&it[5 + i * 2]);
                        }
                    }
                    if (_verboseLogging)
                        MessageOutput.printf("%s it[4] %d\r\n", TAG, it[4]);
                    break;

                case Command::REQUEST_CELL_BALANCE_STATES:
                    if (_stats->_cellsNumber < DALY_MIN_NUMBER_CELLS || _stats->_cellsNumber > DALY_MAX_NUMBER_CELLS) {
                        break;
                    }
                    for (int i = 0; i < 6; i++)
                        _stats->_cellBalance[i] = it[4 + i];
                    _stats->cellBalanceActive = false;
                    for (int i = 0; i < _stats->_cellsNumber; i++) {
                        if (_stats->_cellBalance[i / 8] & (1 << (i & 7))) {
                            _stats->cellBalanceActive = true;
                            break;
                        }
                    }

                    if (_verboseLogging) {
                        char cellBalance[8 + 1 + 8 + 1 + 8 + 1];
                        for (uint8_t j = 0; j < 3; j++) {
                            for (uint8_t i = 0; i < 8; i++)
                                cellBalance[j * (8 + 1) + i] = (_stats->_cellBalance[j] & (0x80 >> i)) ? '1' : '0';
                            cellBalance[j * (8 + 1) + 8] = ' ';
                        }
                        cellBalance[26] = 0;
                        MessageOutput.printf("%s cellBalance %s\r\n", TAG, cellBalance);
                    }
                    break;

                case Command::REQUEST_FAILURE_CODES: {
                    char bytes[72];
                    for (uint8_t j = 0; j < 7; j++) {
                        _stats->FailureStatus.bytes[j] = it[4 + j];
                        for (uint8_t i = 0; i < 8; i++)
                            bytes[j * (8 + 1) + i] = (it[4 + j] & (128 >> i)) ? '1' : '0';
                        bytes[j * (8 + 1) + 8] = ' ';
                    }
                    bytes[(8 + 1) * 6 + 8] = 0;
                    _stats->_faultCode = it[11];
                    if (_verboseLogging)
                        MessageOutput.printf("%s failure status %s, fault code: %02X\r\n", TAG, bytes, _stats->_faultCode);
                }
                    _stats->Warning.lowVoltage = _stats->FailureStatus.levelOneCellVoltageTooLow || _stats->FailureStatus.levelOnePackVoltageTooLow;
                    _stats->Warning.highVoltage = _stats->FailureStatus.levelOneCellVoltageTooHigh || _stats->FailureStatus.levelOnePackVoltageTooHigh;
                    _stats->Warning.lowTemperature = _stats->FailureStatus.levelOneChargeTempTooLow || _stats->FailureStatus.levelOneDischargeTempTooLow;
                    _stats->Warning.highTemperature = _stats->FailureStatus.levelOneChargeTempTooHigh || _stats->FailureStatus.levelOneDischargeTempTooHigh;
                    _stats->Warning.highCurrentCharge = _stats->FailureStatus.levelOneChargeCurrentTooHigh;
                    _stats->Warning.highCurrentDischarge = _stats->FailureStatus.levelOneDischargeCurrentTooHigh;
                    _stats->Warning.cellImbalance = _stats->FailureStatus.levelOneCellVoltageDifferenceTooHigh;

                    _stats->Alarm.underVoltage = _stats->FailureStatus.levelTwoCellVoltageTooLow || _stats->FailureStatus.levelTwoPackVoltageTooLow;
                    _stats->Alarm.overVoltage = _stats->FailureStatus.levelTwoCellVoltageTooHigh || _stats->FailureStatus.levelTwoPackVoltageTooHigh;
                    _stats->Alarm.underTemperature = _stats->FailureStatus.levelTwoChargeTempTooLow || _stats->FailureStatus.levelTwoDischargeTempTooLow;
                    _stats->Alarm.overTemperature = _stats->FailureStatus.levelTwoChargeTempTooHigh || _stats->FailureStatus.levelTwoDischargeTempTooHigh || _stats->FailureStatus.chargeFETTemperatureTooHigh || _stats->FailureStatus.dischargeFETTemperatureTooHigh;
                    _stats->Alarm.overCurrentCharge = _stats->FailureStatus.levelTwoChargeCurrentTooHigh;
                    _stats->Alarm.overCurrentDischarge = _stats->FailureStatus.levelTwoDischargeCurrentTooHigh;
                    _stats->Alarm.bmsInternal = _stats->FailureStatus.failureOfChargeFETTemperatureSensor || _stats->FailureStatus.failureOfDischargeFETTemperatureSensor || _stats->FailureStatus.failureOfMainVoltageSensorModule || _stats->FailureStatus.failureOfChargeFETBreaker || _stats->FailureStatus.failureOfDischargeFETBreaker || _stats->FailureStatus.failureOfEEPROMStorageModule || _stats->FailureStatus.failureOfPrechargeModule || _stats->FailureStatus.failureOfRealtimeClockModule || _stats->FailureStatus.failureOfShortCircuitProtection || _stats->FailureStatus.failureOfTemperatureSensorModule || _stats->FailureStatus.failureOfVoltageSensorModule;
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

    // data from 0x93
    strncpy(_stats->_status, "offline", sizeof(_stats->_status)); // charge/discharge status (0 stationary ,1 charge ,2 discharge)

    _stats->_dIO = 0; // No information about this
    _stats->_bmsCycles = 0; // charge / discharge cycles
    _stats->chargeImmediately1 = false;
    _stats->chargeImmediately2 = false;
    _stats->cellBalanceActive = false;

    // data from 0x95
    // memset(_stats->_cellVoltage, 0, sizeof(_stats->_cellVoltage)); // Store Cell Voltages in mV

    // data from 0x96
    // memset(_stats->_temperature, 0, sizeof(_stats->_temperature)); // array of cell Temperature sensors

    // data from 0x97
    // memset(_stats->_cellBalance, 0, sizeof(_stats->_cellBalance));     // array of cell balance states
    // _stats->cellBalanceActive = false;                                 // bool is cell balance active
}

#endif
