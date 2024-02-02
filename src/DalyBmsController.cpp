#ifdef USE_DALYBMS_CONTROLLER

#include "Configuration.h"
#include "HardwareSerial.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <Arduino.h>
// #include "DalyBmsDataPoints.h"
#include "DalyBmsController.h"
#include <frozen/map.h>

const const char TAG[] = "[Daly BMS]";

HardwareSerial HwSerial(2);

namespace DalyBms {

bool Controller::init()
{
    _verboseLogging = Battery._verbose_logging;

    _lastStatusPrinted.set(10 * 1000);

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("%s rx = %d, tx = %d, rts = %d\r\n", TAG, pin.battery_rx, pin.battery_tx, pin.battery_rts);

    if (pin.battery_rx < 0 || pin.battery_tx < 0 || pin.battery_tx < 0) {
        MessageOutput.printf("%s Invalid RX/TX/RTS pin config\r\n", TAG);
        return false;
    }

    HwSerial.begin(9600, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    HwSerial.flush();

    memset(_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    clearGet();

    _rtsPin = pin.battery_rts;

    pinMode(_rtsPin, OUTPUT);
    digitalWrite(_rtsPin, LOW);

    return true;
}

void Controller::deinit()
{
    HwSerial.end();

    if (_rtsPin > 0) {
        pinMode(_rtsPin, INPUT);
    }
}

frozen::string const& Controller::getStatusText(Controller::Status status)
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

void Controller::announceStatus(Controller::Status status)
{
    if ((_lastStatus == status) && !_lastStatusPrinted.occured()) {
        return;
    }

    MessageOutput.printf("%s %s\r\n", TAG, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

void Controller::sendRequest(uint8_t pollInterval)
{
}

void Controller::loop()
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;
    uint8_t pollInterval = cBattery.PollInterval;

    while (HwSerial.available()) {
        rxData(HwSerial.read());
    }

    sendRequest(pollInterval);

    if (millis() - _lastRequest > 2 * pollInterval * 1000 + 250) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void Controller::sendRequest()
{
    switch (_requestCounter) {
    case 0:
        // _requestCounter = sendCommand() ? (_requestCounter + 1) : 0;
        _requestCounter++;
        break;
    case 1:
        if (getPackMeasurements(true)) {
            _stats->connectionState = true;
            _errorCounter = 0;
            _requestCounter++;
        } else {
            _requestCounter = 0;
            if (_errorCounter < ERRORCOUNTER) {
                _errorCounter++;
            } else {
                _stats->connectionState = false;
                _errorCounter = 0;
                requestCallback();
                // clearGet();
            }
        }
        break;
    case 2:
        _requestCounter = getMinMaxCellVoltage(true) ? (_requestCounter + 1) : 0;
        break;
    case 3:
        _requestCounter = getPackTemp(true) ? (_requestCounter + 1) : 0;
        break;
    case 4:
        _requestCounter = getDischargeChargeMosStatus(true) ? (_requestCounter + 1) : 0;
        break;
    case 5:
        _requestCounter = getStatusInfo(true) ? (_requestCounter + 1) : 0;
        break;
    case 6:
        _requestCounter = getCellVoltages(true) ? (_requestCounter + 1) : 0;
        break;
    case 7:
        _requestCounter = getCellTemperature(true) ? (_requestCounter + 1) : 0;
        break;
    case 8:
        _requestCounter = getCellBalanceState(true) ? (_requestCounter + 1) : 0;
        break;
    case 9:
        _requestCounter = getFailureCodes(true) ? (_requestCounter + 1) : 0;
        if (getStaticData)
            _requestCounter = 0;
        requestCallback();
        break;
    case 10:
        if (!_getStaticData)
            _requestCounter = getVoltageThreshold(true) ? (_requestCounter + 1) : 0;
        _requestCounter = 0;
        requestCallback();
        _getStaticData = true;
        break;

    default:
        break;
    }
}
return;
}

bool Controller::getVoltageThreshold(bool send) // 0x59
{
    if (send) {
        if (requestData(COMMAND::CELL_THRESHOLDS))
            return true;
    } else {
        if (readData(COMMAND::CELL_THRESHOLDS, 1)) {
            _stats->maxCellThreshold1 = (float)((_frameBuff[0][4] << 8) | _frameBuff[0][5]);
            _stats->maxCellThreshold2 = (float)((_frameBuff[0][6] << 8) | _frameBuff[0][7]);
            _stats->minCellThreshold1 = (float)((_frameBuff[0][8] << 8) | _frameBuff[0][9]);
            _stats->minCellThreshold2 = (float)((_frameBuff[0][10] << 8) | _frameBuff[0][11]);

            return true;
        }
    }

    BMS_DEBUG_PRINT("<DALY-BMS DEBUG> Receive failed, min/max cell thresholds won't be modified!\n");
    BMS_DEBUG_WEB("<DALY-BMS DEBUG> Receive failed, min/max cell thresholds won't be modified!\n");

    return false;
}

bool Controller::getMinMaxCellVoltage(bool send) // 0x91
{
    if (send) {
        if (requestData(COMMAND::MIN_MAX_CELL_VOLTAGE))
            return true;
    } else {
        if (requestData(COMMAND::MIN_MAX_CELL_VOLTAGE, 1)) {
            _stats->maxCellmV = (float)((_frameBuff[0][4] << 8) | _frameBuff[0][5]);
            _stats->maxCellVNum = _frameBuff[0][6];
            _stats->minCellmV = (float)((_frameBuff[0][7] << 8) | _frameBuff[0][8]);
            _stats->minCellVNum = _frameBuff[0][9];
            _stats->cellDiff = (_stats->maxCellmV - _stats->minCellmV);

            return true;
        }
    }

    BMS_DEBUG_PRINT("<DALY-BMS DEBUG> Receive failed, min/max cell values won't be modified!\n");
    BMS_DEBUG_WEB("<DALY-BMS DEBUG> Receive failed, min/max cell values won't be modified!\n");

    return false;
}

bool Controller::getPackTemp(bool send) // 0x92
{
    if (send) {
        if (requestData(COMMAND::MIN_MAX_TEMPERATURE))
            return true;
    } else {
        if (readData(COMMAND::MIN_MAX_TEMPERATURE, 1)) {
            _stats->tempAverage = ((_frameBuff[0][4] - 40) + (_frameBuff[0][6] - 40)) / 2;

            return true;
        }
    }

    BMS_DEBUG_PRINT("<DALY-BMS DEBUG> Receive failed, Temp values won't be modified!\n");
    BMS_DEBUG_WEB("<DALY-BMS DEBUG> Receive failed, Temp values won't be modified!\n");

    return false;
}

bool Controller::getDischargeChargeMosStatus(boold send) // 0x93
{
    if (send) {
        if (requestData(COMMAND::DISCHARGE_CHARGE_MOS_STATUS))
            return true;
    } else {
        if (readData(COMMAND::DISCHARGE_CHARGE_MOS_STATUS, 1)) {

            switch (_frameBuff[0][4]) {
            case 0:
                _stats->chargeDischargeStatus = "Stationary";
                break;
            case 1:
                _stats->chargeDischargeStatus = "Charge";
                break;
            case 2:
                _stats->chargeDischargeStatus = "Discharge";
                break;
            }

            _stats->chargeFetState = _frameBuff[0][5];
            _stats->disChargeFetState = _frameBuff[0][6];
            _stats->bmsHeartBeat = _frameBuff[0][7];
            _stats->resCapacitymAh = ((uint32_t)_frameBuff[0][8] << 0x18) | ((uint32_t)_frameBuff[0][9] << 0x10) | ((uint32_t)_frameBuff[0][10] << 0x08) | (uint32_t)_frameBuff[0][11];

            return true;
        }
    }

    BMS_DEBUG_PRINT("<DALY-BMS DEBUG> Receive failed, Charge / discharge mos Status won't be modified!\n");
    BMS_DEBUG_WEB("<DALY-BMS DEBUG> Receive failed, Charge / discharge mos Status won't be modified!\n");

    return false;
}

bool Controller::getStatusInfo(bool send) // 0x94
{
    if (send) {
        if (requestData(COMMAND::STATUS_INFO))
            return true;
    } else {
        if (readData(COMMAND::STATUS_INFO, 1)) {
            _stats->numberOfCells = _frameBuff[0][4];
            _stats->numOfTempSensors = _frameBuff[0][5];
            _stats->chargeState = _frameBuff[0][6];
            _stats->loadState = _frameBuff[0][7];

            // Parse the 8 bits into 8 booleans that represent the states of the Digital IO
            for (size_t i = 0; i < 8; i++) {
                _stats->dIO[i] = bitRead(_frameBuff[0][8], i);
            }

            _stats->bmsCycles = ((uint16_t)_frameBuff[0][9] << 0x08) | (uint16_t)_frameBuff[0][10];

            return true;
        }
    }

    BMS_DEBUG_PRINT("<DALY-BMS DEBUG> Receive failed, Status info won't be modified!\n");
    BMS_DEBUG_WEB("<DALY-BMS DEBUG> Receive failed, Status info won't be modified!\n");

    return false;
}

bool Controller::requestData(COMMAND cmdID) // new function to request global data
{
    // Clear out the buffers
    memset(_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    //--------------send part--------------------
    uint8_t txChecksum = 0x00; // transmit checksum buffer
    // prepare the frame with static data and command ID
    _txBuffer[0] = START_BYTE;
    _txBuffer[1] = HOST_ADRESS;
    _txBuffer[2] = cmdID;
    _txBuffer[3] = DATA_LENGTH; // fixed length

    // Calculate the checksum
    for (uint8_t i = 0; i <= 11; i++) {
        txChecksum += _txBuffer[i];
    }
    // put it on the frame
    _txBuffer[12] = txChecksum;

    if (soft_rts >= 0) {
        digitalWrite(soft_rts, HIGH);
        // wait for the begin of a frame (3.5 symbols)
        delay((int)(3.5 * 11 * 1000 / 9600 + 0.5));
    }

    // send the packet
    HwSerial.write(_txBuffer, XFER_BUFFER_LENGTH);

    if (softrts >= 0) {
        // first wait for transmission end
        HwSerial.flush();

        // wait for the end of a frame (3.5 symbols)
        delay((int)(3.5 * 11 * 1000 / 9600 + 0.5));
        digitalWrite(soft_rts, LOW);
    }

    return true;
}

bool Controller::readData(COMMAND cmdID, unsigned int frameAmount) // new function to request global data
{
    //-----------Recive Part---------------------
    // Clear out the buffers
    memset(_frameBuff, 0x00, sizeof(_frameBuff));
    memset(_rxFrameBuffer, 0x00, sizeof(_rxFrameBuffer));
    unsigned int byteCounter = 0; // bytecounter for incomming data
    /*uint8_t rxByteNum = */
    HWSerial.readBytes(_rxFrameBuffer, XFER_BUFFER_LENGTH * frameAmount);
    for (size_t i = 0; i < frameAmount; i++) {
        for (size_t j = 0; j < XFER_BUFFER_LENGTH; j++) {
            _frameBuff[i][j] = _rxFrameBuffer[byteCounter++];
        }

        uint8_t rxChecksum = 0x00;
        for (int k = 0; k < XFER_BUFFER_LENGTH - 1; k++) {
            rxChecksum += _frameBuff[i][k];
        }
        char debugBuff[128];
        sprintf(debugBuff, "<UART>[Command: 0x%2X][CRC Rec: %2X][CRC Calc: %2X]", cmdID, rxChecksum, _frameBuff[i][XFER_BUFFER_LENGTH - 1]);
        // BMS_DEBUG_PRINTLN(debugBuff);
        // BMS_DEBUG_WEBLN(debugBuff);

        if (rxChecksum != _frameBuff[i][XFER_BUFFER_LENGTH - 1]) {
            BMS_DEBUG_PRINTLN("<UART> CRC FAIL");
            BMS_DEBUG_WEBLN("<UART> CRC FAIL");
            return false;
        }
        if (rxChecksum == 0) {
            BMS_DEBUG_PRINTLN("<UART> NO DATA");
            BMS_DEBUG_WEBLN("<UART> NO DATA");
            return false;
        }
        if (_frameBuff[i][1] >= 0x20) {
            BMS_DEBUG_PRINTLN("<UART> BMS SLEEPING");
            BMS_DEBUG_WEBLN("<UART> BMS SLEEPING");
            return false;
        }
    }
    return true;
}

void Controller::rxData(uint8_t inbyte)
{
}

void Controller::reset()
{
    _buffer.clear();
    return setReadState(ReadState::Idle);
}

void Controller::frameComplete()
{
}

void Controller::clearGet(void)
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
    get.chargeDischargeStatus = "offline"; // charge/discharge status (0 stationary ,1 charge ,2 discharge)

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

} /* namespace DalyBms */

#endif
