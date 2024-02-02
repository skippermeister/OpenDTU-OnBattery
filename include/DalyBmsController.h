#ifdef USE_DALYBMS_CONTROLLER

#pragma once

#include <TimeoutHelper.h>
#include <frozen/string.h>
#include <memory>
#include <vector>

#include "Battery.h"

namespace DalyBms {

class Controller : public BatteryProvider {
public:
    Controller() = default;

    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    const uint8_t XFER_BUFFER_LENGTH = 13;
    const uint8_t MIN_NUMBER_CELLS = 1;
    const uint8_t MAX_NUMBER_CELLS = 48;
    const uint8_t MIN_NUMBER_TEMP_SENSORS = 1;
    const uint8_t MAX_NUMBER_TEMP_SENSORS = 16;

    const uint8_t START_BYTE = 0xA5; // Start byte
    const uint8_t HOST_ADRESS = 0x40; // Host address
    const uint8_t DATA_LENGTH = 8; // Length
    const uint8_t ERRORCOUNTER = 10; // number of try befor clear data

    frozen::string const& getStatusText(Status status);
    void announceStatus(Status status);
    void sendRequest(uint8_t pollInterval);
    bool requestData(COMMAND cmdID);
    bool readData(COMMAND cmdID, unsigned int frameAmount);
    void rxData(uint8_t inbyte);
    void reset();
    void frameComplete();
    void clearGet(void);

    bool _verboseLogging = true;
    int8_t _rtsPin = -1;
    Status _lastStatus = Status::Initializing;
    TimeoutHelper _lastStatusPrinted;

    uint32_t _lastRequest = 0;

    uint8_t _txBuffer[XFER_BUFFER_LENGTH];
    uint8_t _rxFrameBuffer[XFER_BUFFER_LENGTH * 12];
    uint8_t _frameBuff[12][XFER_BUFFER_LENGTH];
    unsigned int _frameCount;

    std::shared_ptr<DalyBmsBatteryStats> _stats = std::make_shared<DalyBmsBatteryStats>();
};

} /* namespace DalyBms */

#endif
