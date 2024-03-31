// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_VICTRON_SMART_SHUNT

#pragma once

#include "Battery.h"

class VictronSmartShunt : public BatteryProvider {
public:
    bool init() final;
    void deinit() final { }
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }
    bool usesHwPort2() const final { return true; }

private:
    uint32_t _lastUpdate = 0;
    std::shared_ptr<VictronSmartShuntStats> _stats = std::make_shared<VictronSmartShuntStats>();
};

#endif
