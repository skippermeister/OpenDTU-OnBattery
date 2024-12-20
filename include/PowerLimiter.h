// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include "PowerLimiterBatteryInverter.h"
#include <espMqttClient.h>
//#include <Hoymiles.h>
#include <Arduino.h>
#include <atomic>
#include <memory>
#include <functional>
#include <optional>
#include <TaskSchedulerDeclarations.h>
#include <frozen/string.h>

#define PL_UI_STATE_INACTIVE 0
#define PL_UI_STATE_CHARGING 1
#define PL_UI_STATE_USE_SOLAR_ONLY 2
#define PL_UI_STATE_USE_SOLAR_AND_BATTERY 3

#define PL_MODE_ENABLE_NORMAL_OP 0
#define PL_MODE_FULL_DISABLE 1
#define PL_MODE_SOLAR_PT_ONLY 2

class PowerLimiterClass {
public:
    PowerLimiterClass();

    enum class Status : unsigned {
        Initializing,
        DisabledByConfig,
        DisabledByMqtt,
        WaitingForValidTimestamp,
        PowerMeterPending,
        InverterInvalid,
        InverterCmdPending,
        ConfigReload,
        InverterStatsPending,
        UnconditionalSolarPassthrough,
        Stable,

        TemperatureRange,
        BatteryNotInitialized,
        DisconnectFromBattery
    };

    void init(Scheduler& scheduler);
    void triggerReloadingConfig() { _reloadConfigFlag = true; }
    uint8_t getInverterUpdateTimeouts() const;
    uint8_t getPowerLimiterState();
    int32_t getInverterOutput() { return _lastExpectedInverterOutput; }
    bool getFullSolarPassThroughEnabled() const { return _fullSolarPassThroughEnabled; }

    enum class Mode : unsigned {
        Normal = 0,
        Disabled = 1,
        UnconditionalFullSolarPassthrough = 2
    };

    void setMode(Mode m) { _mode = m; }
    Mode getMode() const { return _mode; }
    bool usesBatteryPoweredInverter();
    bool isGovernedInverterProducing();

    // added by skippermeister
    bool isInverterSolarPowered(uint64_t serial);

private:
    void loop();

    Task _loopTask;

    std::atomic<bool> _reloadConfigFlag = true;
    uint16_t _lastExpectedInverterOutput = 0;
    bool _shutdownPending = false;
    Status _lastStatus = Status::Initializing;
    uint32_t _lastStatusPrinted = 0;
    uint32_t _lastCalculation = 0;
    static constexpr uint32_t _calculationBackoffMsDefault = 128;
    uint32_t _calculationBackoffMs = _calculationBackoffMsDefault;
    Mode _mode = Mode::Normal;

    std::deque<std::unique_ptr<PowerLimiterInverter>> _inverters;
    bool _batteryDischargeEnabled = false;
    bool _nighttimeDischarging = false;
    std::pair<bool, uint32_t> _nextInverterRestart = { false, 0 };
    bool _fullSolarPassThroughEnabled = false;
    bool _verboseLogging = false;

    frozen::string const& getStatusText(Status status);
    void announceStatus(Status status);
    bool shutdown(Status status);
    bool shutdown() { return shutdown(_lastStatus); }
    void reloadConfig();
    std::pair<float, char const*> getInverterDcVoltage();
    float getBatteryVoltage(bool log = false);
    uint16_t dcPowerBusToInverterAc(uint16_t dcPower);
    void fullSolarPassthrough(PowerLimiterClass::Status reason);
    int16_t calcConsumption();
    using inverter_filter_t = std::function<bool(PowerLimiterInverter const&)>;
    uint16_t updateInverterLimits(uint16_t powerRequested, inverter_filter_t filter, std::string const& filterExpression);
    uint16_t calcPowerBusUsage(uint16_t powerRequested);
    bool updateInverters();
    uint16_t getSolarPassthroughPower();
    std::optional<uint16_t> getBatteryDischargeLimit();
    float getBatteryInvertersOutputAcWatts();

    std::optional<float> _oLoadCorrectedVoltage = std::nullopt;
    float getLoadCorrectedVoltage();

    bool testThreshold(float socThreshold, float voltThreshold, std::function<bool(float, float)> compare);
    bool isStartThresholdReached();
    bool isStopThresholdReached();
    bool isBelowStopThreshold();
    void calcNextInverterRestart();
    bool isFullSolarPassthroughActive();

    void switchMosFetsOff();
    bool manageBatteryDCpowerSwitch();
//    bool _lastDCState = false;
    uint32_t _switchMosFetOffTimer;
    int8_t _preChargePowerState;
    uint32_t _preChargeDelay = 0;
    uint32_t _lastPreCharge = 0;
};

extern PowerLimiterClass PowerLimiter;
