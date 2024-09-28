// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_VICTRON_SMART_BATTERY_SENSE

#pragma once

#include "Battery.h"

class VictronSmartBatterySense : public BatteryProvider {
public:
    bool init() final { return true; };
    void deinit() final {}; // nothing to deinitialize, we use the MPPT interface
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    uint32_t _lastUpdate = 0;
    std::shared_ptr<VictronSmartBatterySenseStats> _stats =
        std::make_shared<VictronSmartBatterySenseStats>();
};
#endif
