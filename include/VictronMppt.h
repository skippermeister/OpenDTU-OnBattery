// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>

#include "VeDirectMpptController.h"
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>

class VictronMpptClass {
public:
    VictronMpptClass() = default;
    ~VictronMpptClass() = default;

    void init(Scheduler& scheduler);
    void updateSettings();

    bool isDataValid() const;
    bool isDataValid(size_t idx) const;

    // returns the data age of all controllers,
    // i.e, the youngest data's age is returned.
    uint32_t getDataAgeMillis() const;
    uint32_t getDataAgeMillis(size_t idx) const;

    size_t controllerAmount() const { return _controllers.size(); }
    std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0) const;

    // total output of all MPPT charge controllers in Watts
    int32_t getPowerOutputWatts() const;

    // total panel input power of all MPPT charge controllers in Watts
    int32_t getPanelPowerWatts() const;

    // sum of total yield of all MPPT charge controllers in kWh
    float getYieldTotal() const;

    // sum of today's yield of all MPPT charge controllers in kWh
    float getYieldDay() const;

    // minimum of all MPPT charge controllers' output voltages in V
    float getOutputVoltage() const;

    // returns the state of operation from the first available controller
    int16_t getStateOfOperation() const;

    // the configured value from the first available controller in V
    enum class MPPTVoltage : uint8_t {
            ABSORPTION = 0,
            FLOAT = 1,
            BATTERY = 2
    };
    float getVoltage(MPPTVoltage kindOf) const;

    bool getVerboseLogging(void) { return _verboseLogging; };
    void setVerboseLogging(bool logging) { _verboseLogging = logging; };

private:
    void loop();
    VictronMpptClass(VictronMpptClass const& other) = delete;
    VictronMpptClass(VictronMpptClass&& other) = delete;
    VictronMpptClass& operator=(VictronMpptClass const& other) = delete;
    VictronMpptClass& operator=(VictronMpptClass&& other) = delete;

    Task _loopTask;

    mutable std::mutex _mutex;
    using controller_t = std::unique_ptr<VeDirectMpptController>;
    std::vector<controller_t> _controllers;

    std::vector<String> _serialPortOwners;
    bool initController(int8_t rx, int8_t tx, bool logging, uint8_t instance);

    bool _verboseLogging = false;
};

extern VictronMpptClass VictronMppt;
