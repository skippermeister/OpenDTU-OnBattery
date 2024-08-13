// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_PYTES_CAN_RECEIVER

#include "Configuration.h"
#include "Battery.h"
#include "BatteryCanReceiver.h"
#include <driver/twai.h>

class PytesCanReceiver : public BatteryCanReceiver {
public:
    explicit PytesCanReceiver() {}

    bool init() final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    bool initialized() const final { return _initialized; };

private:
    std::shared_ptr<PytesBatteryStats> _stats = std::make_shared<PytesBatteryStats>();
};

#endif
