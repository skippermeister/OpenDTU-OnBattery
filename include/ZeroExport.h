// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "MqttHandleZeroExport.h"
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <frozen/string.h>

class ZeroExportClass {
public:
    enum class Status : unsigned {
        Initializing,
        DisabledByConfig,
        WaitingForValidTimestamp,
        PowerMeterDisabled,
        PowerMeterTimeout,
        PowerMeterPending,
        InverterInvalid,
        InverterOffline,
        InverterCommandsDisabled,
        InverterLimitPending,
        InverterPowerCmdPending,
        InverterDevInfoPending,
        InverterStatsPending,
        Settling,
        Stable,
    };

    ZeroExportClass();
    void init(Scheduler& scheduler);
    int16_t getLastRequestedPowerLimit(void) { return _lastRequestedPowerLimit; }

    bool getVerboseLogging(void) { return _verboseLogging; };
    void setVerboseLogging(bool logging) { _verboseLogging = logging; };

    void setParameter(float value, MqttHandleZeroExportClass::Topic parameter);

private:
    void loop();

    Task _loopTask;

    Status _lastStatus = Status::Initializing;
    frozen::string const& getStatusText(Status status);
    void announceStatus(Status status, bool forceLogging = false);
    TimeoutHelper _lastStatusPrinted;

    int16_t pid_Regler(void);
    void commitPowerLimit(std::shared_ptr<InverterAbstract> inverter, int16_t limit, bool enablePowerProduction);
    bool setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int16_t newPowerLimit);

    std::shared_ptr<InverterAbstract> _inverter = nullptr;

    uint16_t _totalMaxPower = 0;
    uint8_t _invID = 0;

    float _actual_I;
    float _last_I;
    int16_t _last_payload;
    uint32_t _last_timeStamp;
    uint32_t _timeStamp;

    static constexpr uint32_t _calculationBackoffMsDefault = 128;
    TimeoutHelper _calculationBackoffMs[INV_MAX_COUNT];

    int16_t _lastRequestedPowerLimit;

    bool _verboseLogging = false;
};

extern ZeroExportClass ZeroExport;
