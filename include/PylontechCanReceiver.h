// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYLONTECH_CAN_RECEIVER

#pragma once

#include "Battery.h"
#include "Configuration.h"
#include <Arduino.h>
#include <driver/twai.h>
#include <espMqttClient.h>
#include <memory>

#ifndef PYLONTECH_PIN_RX
#define PYLONTECH_PIN_RX 27
#endif

#ifndef PYLONTECH_PIN_TX
#define PYLONTECH_PIN_TX 26
#endif

class PylontechCanReceiver : public BatteryProvider {
public:
    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    uint16_t readUnsignedInt16(uint8_t* data) { return ((*(data + 1)) << 8) + *data; };
    int16_t readSignedInt16(uint8_t* data) { return this->readUnsignedInt16(data); };
    //    void readString(char* str, uint8_t numBytes);
    //    void readBooleanBits8(bool* b, uint8_t numBits);
    float scaleValue(int16_t value, float factor) { return value * factor; };
    bool getBit(uint8_t value, uint8_t bit) { return (value & (1 << bit)) >> bit; };

    void dummyData();

    std::shared_ptr<PylontechCanBatteryStats> _stats = std::make_shared<PylontechCanBatteryStats>();
};
#endif
