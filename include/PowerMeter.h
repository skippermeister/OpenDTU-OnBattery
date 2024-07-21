// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "PowerMeterProvider.h"
#include <TaskSchedulerDeclarations.h>
#include <memory>
#include <mutex>

class PowerMeterClass {
public:
    void init(Scheduler& scheduler);

    void updateSettings();

    float getPowerTotal() const;
    float getHousePower() const;
    uint32_t getLastUpdate() const;
    bool isDataValid() const;
    bool getVerboseLogging() {return _verboseLogging; };
    void setVerboseLogging(bool val) { _verboseLogging = val; };

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<PowerMeterProvider> _upProvider = nullptr;

    bool _verboseLogging = false;
};

extern PowerMeterClass PowerMeter;
