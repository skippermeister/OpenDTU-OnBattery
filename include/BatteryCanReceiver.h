// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)

#include "Battery.h"
#include <driver/twai.h>
#include <SPI.h>
#include <mcp_can.h>
#include <Arduino.h>

class BatteryCanReceiver : public BatteryProvider {
public:
    bool init(char const* providerName);
    void deinit() final;
    void loop() final;

    virtual void onMessage(twai_message_t rx_message) = 0;

protected:
    uint8_t readUnsignedInt8(uint8_t *data);
    uint16_t readUnsignedInt16(uint8_t *data);
    int16_t readSignedInt16(uint8_t *data);
    uint32_t readUnsignedInt32(uint8_t *data);
    float scaleValue(int16_t value, float factor);
    bool getBit(uint8_t value, uint8_t bit);

    bool _isCAN0;
    char const* _providerName = "Battery CAN";

    bool _initialized = false;

private:
    int _mcp2515_irq;
    SPIClass *SPI = nullptr;
    MCP_CAN  *_CAN = nullptr;
};

#endif
