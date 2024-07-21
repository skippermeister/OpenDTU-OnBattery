#ifdef USE_JKBMS_CONTROLLER

#include <Arduino.h>
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "JkBmsDataPoints.h"
#include "JkBmsController.h"
#include "SerialPortManager.h"
#include <driver/uart.h>
#include <frozen/map.h>

static constexpr char TAG[] = "[JK BMS]";

namespace JkBms {

bool Controller::init()
{
    _lastStatusPrinted.set(10 * 1000);

    std::string ifcType = "transceiver";
    if (Interface::Transceiver != getInterface()) { ifcType = "TTL-UART"; }
    MessageOutput.printf("%s Initialize %s interface... ", TAG, ifcType.c_str());

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("%s rx = %d, tx = %d, rts = %d\r\n", TAG, pin.battery_rx, pin.battery_tx, pin.battery_rts);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.printf("%s Invalid RX/TX pin config\r\n", TAG);
        return false;
    }

#ifdef JKBMS_DUMMY_SERIAL
    _upSerial = std::make_unique<DummySerial>();
#else
    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    _upSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);
#endif

    _upSerial->begin(115200, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    _upSerial->flush();

    if (Interface::Transceiver != getInterface()) {
        return true;
    }

    if (pin.battery_rts < 0) {
        MessageOutput.printf("%s Invalid transceiver pin config\r\n", TAG);
        return false;
    }

    _upSerial->setPins(pin.battery_rx, pin.battery_tx, UART_PIN_NO_CHANGE, pin.battery_rts);
#ifndef JKBMS_DUMMY_SERIAL
    ESP_ERROR_CHECK(uart_set_mode(*oHwSerialPort, UART_MODE_RS485_HALF_DUPLEX));
#endif

    _initialized = true;

    return true;
}

void Controller::deinit()
{
    _upSerial->end();

    if (PinMapping.get().battery_rts >= 0) { pinMode(PinMapping.get().battery_rts, INPUT); }

    SerialPortManager.freePort(_serialPortOwner);

    MessageOutput.printf("%s Serial driver uninstalled\r\n", TAG);

    _initialized = false;
}

Controller::Interface Controller::getInterface() const
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;
    if (0x00 == cBattery.JkBms.Interface) { return Interface::Uart; }
    if (0x01 == cBattery.JkBms.Interface) { return Interface::Transceiver; }
    return Interface::Invalid;
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
    if (iter == texts.end()) { return missing; }

    return iter->second;
}

void Controller::announceStatus(Controller::Status status)
{
    if ((_lastStatus == status) && !_lastStatusPrinted.occured()) { return; }

    MessageOutput.printf("%s %s\r\n", TAG, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

void Controller::sendRequest(uint8_t pollInterval)
{
    if (ReadState::Idle != _readState) {
        return announceStatus(Status::BusyReading);
    }

    if ((millis() - _lastRequest) < pollInterval * 1000) {
        return announceStatus(Status::WaitingForPollInterval);
    }

    if (!_upSerial->availableForWrite()) {
        return announceStatus(Status::HwSerialNotAvailableForWrite);
    }

    SerialCommand readAll(SerialCommand::Command::ReadAll);

    _upSerial->write(readAll.data(), readAll.size());

    if (Interface::Transceiver == getInterface()) {
        _upSerial->flush();
    }

    _lastRequest = millis();

    setReadState(ReadState::WaitingForFrameStart);
    return announceStatus(Status::RequestSent);
}

void Controller::loop()
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;
    uint8_t pollInterval = cBattery.JkBms.PollingInterval;

    while (_upSerial->available()) {
        rxData(_upSerial->read());
    }

    sendRequest(pollInterval);

    if (millis() > _lastRequest + 2 * pollInterval * 1000 + 250) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void Controller::rxData(uint8_t inbyte)
{
    _buffer.push_back(inbyte);

    switch(_readState) {
        case ReadState::Idle: // unsolicited message from BMS
        case ReadState::WaitingForFrameStart:
            if (inbyte == 0x4E) {
                return setReadState(ReadState::FrameStartReceived);
            }
            break;
        case ReadState::FrameStartReceived:
            if (inbyte == 0x57) {
                return setReadState(ReadState::StartMarkerReceived);
            }
            break;
        case ReadState::StartMarkerReceived:
            _frameLength = inbyte << 8 | 0x00;
            return setReadState(ReadState::FrameLengthMsbReceived);
            break;
        case ReadState::FrameLengthMsbReceived:
            _frameLength |= inbyte;
            _frameLength -= 2; // length field already read
            return setReadState(ReadState::ReadingFrame);
            break;
        case ReadState::ReadingFrame:
            _frameLength--;
            if (_frameLength == 0) {
                return frameComplete();
            }
            return setReadState(ReadState::ReadingFrame);
            break;
    }

    reset();
}

void Controller::reset()
{
    _buffer.clear();
    return setReadState(ReadState::Idle);
}

void Controller::frameComplete()
{
    announceStatus(Status::FrameCompleted);

    if (_verboseLogging) {
        MessageOutput.printf("%s raw data (%d Bytes):", TAG, _buffer.size());
        for (size_t ctr = 0; ctr < _buffer.size(); ++ctr) {
            if (ctr % 16 == 0) {
               MessageOutput.printf("\r\n%s ", TAG);
             }
            MessageOutput.printf(" %02x", _buffer[ctr]);
        }
        MessageOutput.println();
    }

    auto pResponse = std::make_unique<SerialResponse>(std::move(_buffer), _protocolVersion);
    if (pResponse->isValid()) {
        processDataPoints(pResponse->getDataPoints());
    } // if invalid, error message has been produced by SerialResponse c'tor

    reset();
}

void Controller::processDataPoints(DataPointContainer const& dataPoints)
{
    _stats->updateFrom(dataPoints);

    using Label = JkBms::DataPointLabel;

    auto oProtocolVersion = dataPoints.get<Label::ProtocolVersion>();
    if (oProtocolVersion.has_value()) { _protocolVersion = *oProtocolVersion; }

    if (!_verboseLogging) { return; }

    auto iter = dataPoints.cbegin();
    while ( iter != dataPoints.cend() ) {
        MessageOutput.printf("%s:[%11.3f] %s: %s%s\r\n", TAG,
            static_cast<double>(iter->second.getTimestamp())/1000,
            iter->second.getLabelText().c_str(),
            iter->second.getValueText().c_str(),
            iter->second.getUnitText().c_str());
        ++iter;
    }
}

} /* namespace JkBms */

#endif
