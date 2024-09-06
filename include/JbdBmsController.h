#ifdef USE_JBDBMS_CONTROLLER
#pragma once

#include <memory>
#include <vector>
#include <frozen/string.h>

#include "Battery.h"
#include "JbdBmsSerialMessage.h"
#include "JbdBmsController.h"

class DataPointContainer;

namespace JbdBms {

class Controller : public BatteryProvider {
    public:
        Controller() = default;

        bool init(bool verboseLogging) final;
        void deinit() final;
        void loop() final;
        std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    private:
        static char constexpr _serialPortOwner[] = "JBD BMS";

#ifdef JBDBMS_DUMMY_SERIAL
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
            FrameStartReceived, // 1 Byte: 0xDD
            StateReceived,
            CommandCodeReceived,
            ReadingDataContent,
            DataContentReceived,
            ReadingCheckSum,
            CheckSumReceived,
        };

        ReadState _readState;
        void setReadState(ReadState state) {
            _readState = state;
        }

//        bool _verboseLogging = true;
        Status _lastStatus = Status::Initializing;
        uint32_t _lastStatusPrinted = 0;
        uint32_t _lastRequest = 0;
        uint8_t _dataLength = 0;
        JbdBms::SerialResponse::tData _buffer = {};
        std::shared_ptr<JbdBmsBatteryStats> _stats =
            std::make_shared<JbdBmsBatteryStats>();

        bool _initialized = false;
};

} /* namespace JbdBms */
#endif
