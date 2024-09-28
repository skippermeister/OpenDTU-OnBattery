#ifdef USE_JKBMS_CONTROLLER

#pragma once

#include <memory>
#include <vector>
#include <frozen/string.h>

#include "Battery.h"
#include "JkBmsDataPoints.h"
#include "JkBmsSerialMessage.h"

//#define JKBMS_DUMMY_SERIAL
#include "JkBmsDummy.h"

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

namespace JkBms {

class Controller : public BatteryProvider {
    public:
        Controller() = default;

        bool init() final;
        void deinit() final;
        void loop() final;
        std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

        bool initialized() const final { return _initialized; };

    private:
        static char constexpr _serialPortOwner[] = "JK BMS";

#ifdef JKBMS_DUMMY_SERIAL
        std::unique_ptr<DummySerial> _upSerial;
#else
        std::unique_ptr<HardwareSerial> _upSerial;
#endif

        enum class Status : unsigned {
            Initializing,
            Timeout,
            WaitingForPollInterval,
            HwSerialNotAvailableForWrite,
            BusyReading,
            RequestSent,
            FrameCompleted
        };

        frozen::string const& getStatusText(Status status);
        void announceStatus(Status status);
        void sendRequest(uint8_t pollInterval);
        void rxData(uint8_t inbyte);
        void reset();
        void frameComplete();
        void processDataPoints(DataPointContainer const& dataPoints);

        enum class Interface : unsigned {
            Invalid,
            Uart,
            Transceiver
        };

        enum class ReadState : unsigned {
            Idle,
            WaitingForFrameStart,
            FrameStartReceived,
            StartMarkerReceived,
            FrameLengthMsbReceived,
            ReadingFrame
        };
        ReadState _readState;
        void setReadState(ReadState state) {
            _readState = state;
        }

        Status _lastStatus = Status::Initializing;
        uint32_t _lastStatusPrinted = 0;
        uint32_t _lastRequest = 0;
        uint16_t _frameLength = 0;
        uint8_t _protocolVersion = -1;
        SerialResponse::tData _buffer = {};
        std::shared_ptr<JkBmsBatteryStats> _stats = std::make_shared<JkBmsBatteryStats>();

        bool _initialized = false;
};

} /* namespace JkBms */
#endif
