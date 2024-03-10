// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <Arduino.h>
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <espMqttClient.h>
#include <frozen/string.h>
#include <functional>
#include <memory>

#define PL_UI_STATE_INACTIVE 0
#define PL_UI_STATE_CHARGING 1
#define PL_UI_STATE_USE_SOLAR_ONLY 2
#define PL_UI_STATE_USE_SOLAR_AND_BATTERY 3

#define PL_MODE_ENABLE_NORMAL_OP 0
#define PL_MODE_FULL_DISABLE 1
#define PL_MODE_SOLAR_PT_ONLY 2

typedef enum {
    SHUTDOWN = 0,
    ACTIVE
} plStates;

typedef enum {
    EMPTY_WHEN_FULL = 0,
    EMPTY_AT_NIGHT
} batDrainStrategy;

class PowerLimiterClass {
public:
    enum class Status : unsigned {
        Initializing,
        DisabledByConfig,
        DisabledByMqtt,
        WaitingForValidTimestamp,
        PowerMeterDisabled,
        PowerMeterTimeoutWarning,
        PowerMeterTimeout,
        PowerMeterPending,
        InverterInvalid,
        InverterChanged,
        InverterOffline,
        InverterCommandsDisabled,
        InverterLimitPending,
        InverterPowerCmdPending,
        InverterDevInfoPending,
        InverterStatsPending,
        UnconditionalSolarPassthrough,
        NoVeDirect,
        Settling,
        Stable,
        LowerLimitUndercut,
        TemperatureRange,
        BatteryNotInitialized,
        DisconnectFromBattery
        //        WaitingInverterPowerOn
    };

    PowerLimiterClass();
    void init(Scheduler& scheduler);
    uint8_t getPowerLimiterState();
    int32_t getLastRequestedPowerLimit();

    enum class Mode : unsigned {
        Normal = 0,
        Disabled = 1,
        UnconditionalFullSolarPassthrough = 2
    };

    void setMode(Mode m) { _mode = m; }
    Mode getMode() const { return _mode; }
    void calcNextInverterRestart();

    bool getVerboseLogging(void) { return _verbose_logging; };
    void setVerboseLogging(bool logging) { _verbose_logging = logging; };

private:
    void loop();

    Task _loopTask;

    int32_t _lastRequestedPowerLimit = 0;
    uint32_t _lastPowerLimitMillis = 0;
    uint32_t _shutdownTimeout = 0;
    bool _shutdownInProgress;
    Status _lastStatus = Status::Initializing;
    TimeoutHelper _lastStatusPrinted;
    uint32_t _lastCalculation = 0;
    static constexpr uint32_t _calculationBackoffMsDefault = 128;
    uint32_t _calculationBackoffMs = _calculationBackoffMsDefault;
    Mode _mode = Mode::Normal;
    std::shared_ptr<InverterAbstract> _inverter = nullptr;
    bool _batteryDischargeEnabled = false;
    uint32_t _nextInverterRestart = 0; // Values: 0->not calculated / 1->no restart configured / >1->time of next inverter restart in millis()
    uint32_t _nextCalculateCheck = 5000; // time in millis for next NTP check to calulate restart
    bool _fullSolarPassThroughEnabled = false;

    frozen::string const& getStatusText(Status status);
    void announceStatus(Status status);
    bool shutdown(Status status);
    bool shutdown() { return shutdown(_lastStatus); }
    float getBatteryVoltage(bool log = false);
    int32_t inverterPowerDcToAc(std::shared_ptr<InverterAbstract> inverter, int32_t dcPower);
    void unconditionalSolarPassthrough(std::shared_ptr<InverterAbstract> inverter);
    bool canUseDirectSolarPower();
    int32_t calcPowerLimit(std::shared_ptr<InverterAbstract> inverter, bool solarPowerEnabled, bool batteryDischargeEnabled);
    void commitPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t limit, bool enablePowerProduction);
    bool setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newPowerLimit);
    int32_t getSolarChargePower();
    float getLoadCorrectedVoltage();
    bool testThreshold(float socThreshold, float voltThreshold,
        std::function<bool(float, float)> compare);
    bool isStartThresholdReached();
    bool isStopThresholdReached();
    bool isBelowStopThreshold();
    bool useFullSolarPassthrough();

    bool manageBatteryDCpowerSwitch();
//    bool _lastDCState = false;

    int8_t _preChargePowerState;
    uint32_t _preChargeDelay = 0;
    uint32_t _lastPreCharge = 0;
    bool _verbose_logging = false;
};

extern PowerLimiterClass PowerLimiter;
