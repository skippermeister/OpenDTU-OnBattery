// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYLONTECH_CAN_RECEIVER

#pragma once

#include "Configuration.h"
#include "Battery.h"
#include "BatteryCanReceiver.h"
#include <driver/twai.h>
#include <espMqttClient.h>
#include <Arduino.h>

class PylontechCanReceiver : public BatteryProvider {
public:
    bool init() final;
    void onMessage(twai_message_t rx_message) final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    void dummyData();

    std::shared_ptr<PylontechCanBatteryStats> _stats = std::make_shared<PylontechCanBatteryStats>();
};
#endif
