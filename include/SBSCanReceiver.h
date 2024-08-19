// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_SBS_CAN_RECEIVER

#include "BatteryCanReceiver.h"
#include <driver/twai.h>
#include <Arduino.h>

class SBSCanReceiver : public BatteryCanReceiver {
public:
    bool init() final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    bool initialized() const final { return _initialized; };

private:
    void dummyData();
    std::shared_ptr<SBSBatteryStats> _stats = std::make_shared<SBSBatteryStats>();
};

#endif
