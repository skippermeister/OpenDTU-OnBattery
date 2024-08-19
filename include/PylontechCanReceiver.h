// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_PYLONTECH_CAN_RECEIVER

#include "Configuration.h"
#include "Battery.h"
#include "BatteryCanReceiver.h"
#include <driver/twai.h>
#include <espMqttClient.h>
#include <Arduino.h>

class PylontechCanReceiver : public BatteryCanReceiver  {
public:
    PylontechCanReceiver() {}

    bool init() final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    bool initialized() const final { return _initialized; };

private:
    void dummyData();

    std::shared_ptr<PylontechCanBatteryStats> _stats = std::make_shared<PylontechCanBatteryStats>();
};
#endif
