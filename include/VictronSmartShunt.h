// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Battery.h"

class VictronSmartShunt : public BatteryProvider {
public:
    bool init() final;
    void deinit() final { }
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    uint32_t _lastUpdate = 0;
    std::shared_ptr<VictronSmartShuntStats> _stats = std::make_shared<VictronSmartShuntStats>();
};
