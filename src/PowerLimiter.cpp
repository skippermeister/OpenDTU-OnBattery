// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#include "PowerLimiter.h"
#include "Battery.h"
#include "Configuration.h"
#include "Huawei_can.h"
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "PinMapping.h"
#include "PowerMeter.h"
#include "SunPosition.h"
#include <VictronMppt.h>
#include <cmath>
#include <ctime>
#include <frozen/map.h>

static constexpr char TAG[] = "[PowerLimiter]";

PowerLimiterClass PowerLimiter;

PowerLimiterClass::PowerLimiterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&PowerLimiterClass::loop, this))
{
}

void PowerLimiterClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Power Limiter... ");

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _lastStatusPrinted.set(10 * 1000);

    const PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    MessageOutput.printf("switch DC power of inverter channel %d off. ", cPL.InverterChannelId);
    digitalWrite(PinMapping.get().pre_charge, HIGH);
    pinMode(PinMapping.get().pre_charge, OUTPUT);
    digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
    digitalWrite(PinMapping.get().full_power, HIGH);
    pinMode(PinMapping.get().full_power, OUTPUT);
    digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
    _preChargePowerState = 0;
    _lastPreCharge = millis();
    _preChargeDelay = 0;

    if (Configuration.get().Inverter[cPL.InverterId].Serial > 0) {
        // switch PowerMOSFETs for inverter DC power off (battery powered Hoymiles inverter)
        Hoymiles.getInverterByPos(cPL.InverterId)->setConnected(false);
        MessageOutput.println("done");
    } else {
        MessageOutput.println("no Inverter for Powerlimiter configured");
    }
}

frozen::string const& PowerLimiterClass::getStatusText(PowerLimiterClass::Status status)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Status, frozen::string, 24> texts = {
        { Status::Initializing, "initializing (should not see me)" },
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::DisabledByMqtt, "disabled by MQTT" },
        { Status::WaitingForValidTimestamp, "waiting for valid date and time to be available" },
        { Status::PowerMeterDisabled, "no power meter is configured/enabled" },
        { Status::PowerMeterTimeoutWarning, "warning, power meter readings are outdated more than 30 seconds" },
        { Status::PowerMeterTimeout, "power meter readings are outdated more than 1 minute" },
        { Status::PowerMeterPending, "waiting for sufficiently recent power meter reading" },
        { Status::InverterInvalid, "invalid inverter selection/configuration" },
        { Status::InverterChanged, "target inverter changed" },
        { Status::InverterOffline, "inverter is offline (polling enabled? radio okay?)" },
        { Status::InverterCommandsDisabled, "inverter configuration prohibits sending commands" },
        { Status::InverterLimitPending, "waiting for a power limit command to complete" },
        { Status::InverterPowerCmdPending, "waiting for a start/stop/restart command to complete" },
        { Status::InverterDevInfoPending, "waiting for inverter device information to be available" },
        { Status::InverterStatsPending, "waiting for sufficiently recent inverter data" },
        { Status::UnconditionalSolarPassthrough, "unconditionally passing through all solar power (MQTT override)" },
        { Status::NoVeDirect, "VE.Direct disabled, connection broken, or data outdated" },
        { Status::Settling, "waiting for the system to settle" },
        { Status::Stable, "the system is stable, the last power limit is still valid" },
        { Status::LowerLimitUndercut, "calculated power limit undercuts configured lower limit" },
        { Status::TemperatureRange, "temperatue out of recommended discharge range (-10°C ~ 50°C)" },
        { Status::BatteryNotInitialized, "battery is not initialized" },
        { Status::DisconnectFromBattery, "disconnect inverter from battery" }
        //        { Status::WaitingInverterPowerOn, "waiting till Inverter is startet (DC power on)"}
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) {
        return missing;
    }

    return iter->second;
}

void PowerLimiterClass::announceStatus(PowerLimiterClass::Status status)
{
    // this method is called with high frequency. print the status text if
    // the status changed since we last printed the text of another one.
    // otherwise repeat the info with a fixed interval.
    if (_lastStatus == status && !_lastStatusPrinted.occured()) {
        return;
    }

    // after announcing once that the DPL is disabled by configuration, it
    // should just be silent while it is disabled.
    if (status == Status::DisabledByConfig && _lastStatus == status) {
        return;
    }

    MessageOutput.printf("%s %s\r\n", TAG, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

bool PowerLimiterClass::shutdown(PowerLimiterClass::Status status)
{
    if (_lastStatus != Status::DisconnectFromBattery)
        announceStatus(status);

    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    if (_inverter == nullptr || !_inverter->isProducing() || (_shutdownTimeout > 0 && (millis() - _shutdownTimeout > 30 * 1000))) {

        if (_preChargePowerState != 0 && (millis() - _shutdownTimeout > 30 * 1000)) { // at minimum wait 30 seconds to switch off the MOSfets

            if (_inverter)
                MessageOutput.printf("%s%s inverter %d is %s producing. AC power: %.1f\r\n", TAG, __FUNCTION__,
                    cPL.InverterId,
                    _inverter->isProducing() ? "still" : "not",
                    _inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));

            MessageOutput.printf("%s%s: switch DC power of inverter id %d at channel %d OFF\r\n", TAG, __FUNCTION__,
                cPL.InverterId,
                cPL.InverterChannelId);

            digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
            digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off

            _preChargePowerState = 0;
            if (_inverter)
                _inverter->setConnected(false);

            _lastPreCharge = millis();
            _preChargeDelay = 0;

            _inverter = nullptr;
            _shutdownTimeout = 0;
            return false;

        } else if (_preChargePowerState == 0) {

            _inverter = nullptr;
            _shutdownTimeout = 0;
            return false;
        }

        return true;
    }

    if (!_inverter->isReachable()) {
        return true;
    } // retry later (until timeout)

    if (_shutdownTimeout > 0 && _inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC) > 0.0) {
        return true;
    }

    // retry shutdown for a maximum amount of time before giving up
    if (_shutdownTimeout == 0) {
        _shutdownTimeout = millis();
        if (_shutdownTimeout == 0)
            _shutdownTimeout++;
    }

    auto lastLimitCommandState = _inverter->SystemConfigPara()->getLastLimitCommandSuccess();
    if (CMD_PENDING == lastLimitCommandState) {
        return true;
    }

    auto lastPowerCommandState = _inverter->PowerCommand()->getLastPowerCommandSuccess();
    if (CMD_PENDING == lastPowerCommandState) {
        return true;
    }

    commitPowerLimit(_inverter, cPL.LowerPowerLimit, false);

    return true;
}

void PowerLimiterClass::loop()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    // we know that the Hoymiles library refuses to send any message to any
    // inverter until the system has valid time information. until then we can
    // do nothing, not even shutdown the inverter.
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) {
        return announceStatus(Status::WaitingForValidTimestamp);
    }

    if (_shutdownTimeout > 0) {
        // we transition from SHUTDOWN to OFF when we know the inverter was
        // shut down. until then, we retry shutting it down. in this case we
        // preserve the original status that lead to the decision to shut down.
        shutdown();
        return;
    }

    if (!cPL.Enabled) {
        shutdown(Status::DisabledByConfig);
        //        _lastDCState = false;
        return;
    }

    if (!Battery.getStats()->isDischargeTemperatureValid()) {
        shutdown(Status::TemperatureRange);
        return;
    }

    if (!Battery.getStats()->initialized()) {
        shutdown(Status::BatteryNotInitialized);
        return;
    }

    if (Mode::Disabled == _mode) {
        shutdown(Status::DisabledByMqtt);
        return;
    }

    std::shared_ptr<InverterAbstract> currentInverter = Hoymiles.getInverterByPos(cPL.InverterId);

    // in case of (newly) broken configuration, shut down
    // the last inverter we worked with (if any)
    if (currentInverter == nullptr) {
        shutdown(Status::InverterInvalid);
        return;
    }

    // if the DPL is supposed to manage another inverter now, we first
    // shut down the previous one, if any. then we pick up the new one.
    if (_inverter != nullptr && _inverter->serial() != currentInverter->serial()) {
        shutdown(Status::InverterChanged);
        return;
    }

    // update our pointer as the configuration might have changed
    _inverter = currentInverter;

    if (manageBatteryDCpowerSwitch() == false)
        return;

    // data polling is disabled or the inverter is deemed offline
    if (!_inverter->isReachable()) {
        return announceStatus(Status::InverterOffline);
    }

    // sending commands to the inverter is disabled
    if (!_inverter->getEnableCommands()) {
        return announceStatus(Status::InverterCommandsDisabled);
    }

    // concerns active power commands (power limits) only (also from web app or MQTT)
    auto lastLimitCommandState = _inverter->SystemConfigPara()->getLastLimitCommandSuccess();
    if (CMD_PENDING == lastLimitCommandState) {
        return announceStatus(Status::InverterLimitPending);
    }

    // concerns power commands (start, stop, restart) only (also from web app or MQTT)
    auto lastPowerCommandState = _inverter->PowerCommand()->getLastPowerCommandSuccess();
    if (CMD_PENDING == lastPowerCommandState) {
        return announceStatus(Status::InverterPowerCmdPending);
    }

    // a calculated power limit will always be limited to the reported
    // device's max power. that upper limit is only known after the first
    // DevInfoSimpleCommand succeeded.
    if (_inverter->DevInfo()->getMaxPower() <= 0) {
        return announceStatus(Status::InverterDevInfoPending);
    }

    if (Mode::UnconditionalFullSolarPassthrough == _mode) {
        // handle this mode of operation separately
        return unconditionalSolarPassthrough(_inverter);
    }

    // the normal mode of operation requires a valid
    // power meter reading to calculate a power limit
    if (!Configuration.get().PowerMeter.Enabled) {
        shutdown(Status::PowerMeterDisabled);
        return;
    }

    // after 30 seconds no power meter values we warn,
    // after 1 minute no power meter values we shutdown the inverter
    // it seems to be that shelly3EM sometimes logs off from WLAN and reports for more than 30 seconds no values
    if (millis() - PowerMeter.getLastPowerMeterUpdate() > (30 * 1000)) {
        return announceStatus(Status::PowerMeterTimeoutWarning);
    } else if (millis() - PowerMeter.getLastPowerMeterUpdate() > (60 * 1000)) {
        shutdown(Status::PowerMeterTimeout);
        return;
    }

    // concerns both power limits and start/stop/restart commands and is
    // only updated if a respective response was received from the inverter
    auto lastUpdateCmd = std::max(
        _inverter->SystemConfigPara()->getLastUpdateCommand(),
        _inverter->PowerCommand()->getLastUpdateCommand());

    // wait for power meter and inverter stat updates after a settling phase
    if (millis() - lastUpdateCmd < 3 * 1000) {
        return announceStatus(Status::Settling);
    }

    if (_inverter->Statistics()->getLastUpdate() - lastUpdateCmd <= 3 * 1000) {
        return announceStatus(Status::InverterStatsPending);
    }

    if (PowerMeter.getLastPowerMeterUpdate() - lastUpdateCmd <= 3 * 1000) {
        return announceStatus(Status::PowerMeterPending);
    }

    // since _lastCalculation and _calculationBackoffMs are initialized to
    // zero, this test is passed the first time the condition is checked.
    if (millis() - _lastCalculation < _calculationBackoffMs) {
        return announceStatus(Status::Stable);
    }

    if (_verbose_logging) {
        MessageOutput.printf("%s ******************* ENTER **********************\r\n", TAG);
    }

    // Check if next inverter restart time is reached
    if ((_nextInverterRestart > 1) && (millis() - _nextInverterRestart > 0)) {
        MessageOutput.printf("%s%s: send inverter restart\r\n", TAG, __FUNCTION__);
        _inverter->sendRestartControlRequest();
        calcNextInverterRestart();
    }

    // Check if NTP time is set and next inverter restart not calculated yet
    if ((cPL.RestartHour >= 0) && (_nextInverterRestart == 0)) {
        // check every 5 seconds
        if (millis() - _nextCalculateCheck > 5 * 1000) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 5)) {
                calcNextInverterRestart();
            } else {
                MessageOutput.printf("%s%s: inverter restart calculation: NTP not ready\r\n", TAG, __FUNCTION__);
                _nextCalculateCheck = millis();
            }
        }
    }

    // Battery charging cycle conditions
    // First we always disable discharge if the battery is empty
    if (isStopThresholdReached()) {
        // Disable battery discharge when empty
        if (_verbose_logging)
            MessageOutput.printf("%s%s: Stop threshold reached (no discharging)\r\n", TAG, __FUNCTION__);
        _batteryDischargeEnabled = false;
    } else {
        // UI: Solar Passthrough Enabled -> false
        // Battery discharge can be enabled when start threshold is reached
        if (!cPL.SolarPassThroughEnabled && isStartThresholdReached()) {
            if (_verbose_logging)
                MessageOutput.printf("%s%s: Solar passthroug is disabled and start threshold reached (discharging allowed)\r\n", TAG, __FUNCTION__);
            _batteryDischargeEnabled = true;
        }

        // UI: Solar Passthrough Enabled -> true && EMPTY_AT_NIGHT
        if (cPL.SolarPassThroughEnabled && cPL.BatteryDrainStategy == EMPTY_AT_NIGHT) {
            if (isStartThresholdReached()) {
                // In this case we should only discharge the battery as long it is above startThreshold
                if (_verbose_logging)
                    MessageOutput.printf("%s%s: Start threshold reached (discharging)\r\n", TAG, __FUNCTION__);
                _batteryDischargeEnabled = true;
            } else {
                // In this case we should only discharge the battery when there is no sunshine
                _batteryDischargeEnabled = !canUseDirectSolarPower();
                if (_verbose_logging)
                    MessageOutput.printf("%s%s: discharging is %s\r\n", TAG, __FUNCTION__, _batteryDischargeEnabled ? "allowed" : "prevented");
            }
        }

        // UI: Solar Passthrough Enabled -> true && EMPTY_WHEN_FULL
        // Battery discharge can be enabled when start threshold is reached
        if (cPL.SolarPassThroughEnabled && isStartThresholdReached() && cPL.BatteryDrainStategy == EMPTY_WHEN_FULL) {
            if (_verbose_logging)
                MessageOutput.printf("%s%s: Solar pass through, start threshold reached, EMPTY_WHEN_FULL\r\n", TAG, __FUNCTION__);
            _batteryDischargeEnabled = true;
        }
    }

    if (_verbose_logging) {
        MessageOutput.printf("%s%s: battery interface %s, SoC: %.1f%%, StartTH: %d %%, StopTH: %d %%, SoC age: %d s, ignore: %s\r\n", TAG, __FUNCTION__,
            (Configuration.get().Battery.Enabled ? "enabled" : "disabled"),
            Battery.getStats()->getSoC(),
            cPL.BatterySocStartThreshold,
            cPL.BatterySocStopThreshold,
            Battery.getStats()->getSoCAgeSeconds(),
            cPL.IgnoreSoc ? "yes" : "no");

        auto dcVoltage = getBatteryVoltage(true /*log voltages only once per DPL loop*/);
        MessageOutput.printf("%s%s: dcVoltage: %.2f V, loadCorrectedVoltage: %.2f V, StartTH: %.2f V, StopTH: %.2f V\r\n", TAG, __FUNCTION__,
            dcVoltage, getLoadCorrectedVoltage(),
            cPL.VoltageStartThreshold,
            cPL.VoltageStopThreshold);

        MessageOutput.printf("%s%s: StartTH reached: %s, StopTH reached: %s, inverter %s producing\r\n", TAG, __FUNCTION__,
            (isStartThresholdReached() ? "yes" : "no"),
            (isStopThresholdReached() ? "yes" : "no"),
            (_inverter->isProducing() ? "is" : "is NOT"));

        MessageOutput.printf("%s%s: SolarPT %s, Drain Strategy: %i, canUseDirectSolarPower: %s\r\n", TAG, __FUNCTION__,
            (cPL.SolarPassThroughEnabled ? "enabled" : "disabled"),
            cPL.BatteryDrainStategy, (canUseDirectSolarPower() ? "yes" : "no"));

        MessageOutput.printf("%s%s: battery discharging %s, PowerMeter: %d W, target consumption: %d W\r\n", TAG, __FUNCTION__,
            (_batteryDischargeEnabled ? "allowed" : "prevented"),
            static_cast<int32_t>(round(PowerMeter.getPowerTotal())),
            cPL.TargetPowerConsumption);
    }

    // Calculate and set Power Limit
    int32_t newPowerLimit = calcPowerLimit(_inverter, canUseDirectSolarPower(), _batteryDischargeEnabled);
    bool limitUpdated = setNewPowerLimit(_inverter, newPowerLimit);

    if (_verbose_logging) {
        MessageOutput.printf("%s ******************* Leaving PL, calculated limit: %d W, requested limit: %d W (%s)\r\n", TAG,
            newPowerLimit, _lastRequestedPowerLimit,
            (limitUpdated ? "updated from calculated" : "kept last requested"));
    }

    _lastCalculation = millis();

    if (!limitUpdated) {
        // increase polling backoff if system seems to be stable
        _calculationBackoffMs = std::min<uint32_t>(1024, _calculationBackoffMs * 2);
        return announceStatus(Status::Stable);
    }

    _calculationBackoffMs = _calculationBackoffMsDefault;
}

/**
 * determines the battery's voltage, trying multiple data providers. the most
 * accurate data is expected to be delivered by a BMS, if it's available. more
 * accurate and more recent than the inverter's voltage reading is the volage
 * at the charge controller's output, if it's available. only as a fallback
 * the voltage reported by the inverter is used.
 */
float PowerLimiterClass::getBatteryVoltage(bool log)
{
    if (!_inverter) {
        // there should be no need to call this method if no target inverter is known
        MessageOutput.println("DPL getBatteryVoltage: no inverter (programmer error)");
        return 0.0;
    }

    auto const& config = Configuration.get();
    auto channel = static_cast<ChannelNum_t>(config.PowerLimiter.InverterChannelId);
    float inverterVoltage = _inverter->Statistics()->getChannelFieldValue(TYPE_DC, channel, FLD_UDC);
    float res = inverterVoltage;

    float chargeControllerVoltage = -1;
    if (VictronMppt.isDataValid()) {
        res = chargeControllerVoltage = static_cast<float>(VictronMppt.getOutputVoltage());
    }

    float bmsVoltage = -1;
    auto stats = Battery.getStats();
    if (config.Battery.Enabled
        && stats->isVoltageValid()
        && stats->getVoltageAgeSeconds() < 60) {
        res = bmsVoltage = stats->getVoltage();
    }

    if (log) {
        MessageOutput.printf("[DPL::getBatteryVoltage] BMS: %.2f V, MPPT: %.2f V, inverter: %.2f V, returning: %.2fV\r\n",
            bmsVoltage, chargeControllerVoltage, inverterVoltage, res);
    }

    return res;
}

/**
 * calculate the AC output power (limit) to set, such that the inverter uses
 * the given power on its DC side, i.e., adjust the power for the inverter's
 * efficiency.
 */
int32_t PowerLimiterClass::inverterPowerDcToAc(std::shared_ptr<InverterAbstract> inverter, int32_t dcPower)
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    float inverterEfficiencyPercent = inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_EFF);

    // fall back to hoymiles peak efficiency as per datasheet if inverter
    // is currently not producing (efficiency is zero in that case)
    float inverterEfficiencyFactor = (inverterEfficiencyPercent > 0) ? inverterEfficiencyPercent / 100 : 0.967;
    if (_verbose_logging)
        MessageOutput.printf("%s%s: efficency %f, factor %.3f%%\r\n", TAG, __FUNCTION__, inverterEfficiencyPercent, inverterEfficiencyFactor);

    // account for losses between solar charger and inverter (cables, junctions...)
    float lossesFactor = 1.00 - static_cast<float>(cPL.SolarPassThroughLosses) / 100;

    return dcPower * inverterEfficiencyFactor * lossesFactor;
}

/**
 * implements the "unconditional solar passthrough" mode of operation, which
 * can currently only be set using MQTT. in this mode of operation, the
 * inverter shall behave as if it was connected to the solar panels directly,
 * i.e., all solar power (and only solar power) is fed to the AC side,
 * independent from the power meter reading.
 */
void PowerLimiterClass::unconditionalSolarPassthrough(std::shared_ptr<InverterAbstract> inverter)
{
    if (!VictronMppt.isDataValid()) {
        shutdown(Status::NoVeDirect);
        return;
    }

    int32_t solarPower = VictronMppt.getPowerOutputWatts();
    setNewPowerLimit(inverter, inverterPowerDcToAc(inverter, solarPower));
    announceStatus(Status::UnconditionalSolarPassthrough);
}

uint8_t PowerLimiterClass::getPowerLimiterState()
{
    if (_inverter == nullptr || !_inverter->isReachable()) {
        return PL_UI_STATE_INACTIVE;
    }

    if (_inverter->isProducing() && _batteryDischargeEnabled) {
        return PL_UI_STATE_USE_SOLAR_AND_BATTERY;
    }

    if (_inverter->isProducing() && !_batteryDischargeEnabled) {
        return PL_UI_STATE_USE_SOLAR_ONLY;
    }

    if (!_inverter->isProducing()) {
        return PL_UI_STATE_CHARGING;
    }

    return PL_UI_STATE_INACTIVE;
}

int32_t PowerLimiterClass::getLastRequestedPowerLimit()
{
    return _lastRequestedPowerLimit;
}

bool PowerLimiterClass::canUseDirectSolarPower()
{
    if (!Configuration.get().PowerLimiter.SolarPassThroughEnabled
        || isBelowStopThreshold()
        || !VictronMppt.isDataValid()) {
        return false;
    }

    return VictronMppt.getPowerOutputWatts() >= 20; // enough power?
}

// Logic table
// | Case # | batteryDischargeEnabled | solarPowerEnabled | useFullSolarPassthrough | Result                                                      |
// | 1      | false                   | false             | doesn't matter          | PL = 0                                                      |
// | 2      | false                   | true              | doesn't matter          | PL = Victron Power                                          |
// | 3      | true                    | doesn't matter    | false                   | PL = PowerMeter value (Battery can supply unlimited energy) |
// | 4      | true                    | false             | true                    | PL = PowerMeter value                                       |
// | 5      | true                    | true              | true                    | PL = max(PowerMeter value, Victron Power)                   |

int32_t PowerLimiterClass::calcPowerLimit(std::shared_ptr<InverterAbstract> inverter, bool solarPowerEnabled, bool batteryDischargeEnabled)
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    int32_t acPower = 0;
    int32_t newPowerLimit = round(PowerMeter.getPowerTotal());

    if (!solarPowerEnabled && !batteryDischargeEnabled) {
        // Case 1 - No energy sources available
        return 0;
    }

    if (cPL.IsInverterBehindPowerMeter) {
        // If the inverter the behind the power meter (part of measurement),
        // the produced power of this inverter has also to be taken into account.
        // We don't use FLD_PAC from the statistics, because that
        // data might be too old and unreliable.
        //        acPower = static_cast<int>(inverter->Statistics()->getChannelFieldValue(TYPE_AC, (ChannelNum_t) cPL.InverterChannelId, FLD_PAC));
        acPower = static_cast<int>(inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));
        if (_verbose_logging)
            MessageOutput.printf("%s%s: Inverter acPower %d W\r\n", TAG, __FUNCTION__, acPower);
        newPowerLimit += acPower;
    }

    // We're not trying to hit 0 exactly but take an offset into account
    // This means we never fully compensate the used power with the inverter
    // Case 3
    newPowerLimit -= cPL.TargetPowerConsumption;

    // At this point we've calculated the required energy to compensate for household consumption.
    // If the battery is enabled this can always be supplied since we assume that the battery can supply unlimited power
    // The next step is to determine if the Solar power as provided by the Victron charger
    // actually constrains or dictates another inverter power value
    int32_t adjustedVictronChargePower = inverterPowerDcToAc(inverter, getSolarChargePower());

    // Battery can be discharged and we should output max (Victron solar power || power meter value)
    if (batteryDischargeEnabled && useFullSolarPassthrough()) {
        // Case 5
        newPowerLimit = newPowerLimit > adjustedVictronChargePower ? newPowerLimit : adjustedVictronChargePower;
    } else {
        // We check if the PSU is on and disable the Power Limiter in this case.
        // The PSU should reduce power or shut down first before the Power Limiter kicks in
        // The only case where this is not desired is if the battery is over the Full Solar Passthrough Threshold
        // In this case the Power Limiter should start. The PSU will shutdown when the Power Limiter is active
#ifdef CHARGER_HUAWEI
        if (HuaweiCan.getAutoPowerStatus()) {
#else
        if (MeanWellCan.getAutoPowerStatus()) {
#endif
            return 0;
        }
    }

    // We should use Victron solar power only (corrected by efficiency factor)
    if (solarPowerEnabled && !batteryDischargeEnabled) {
        // Case 2 - Limit power to solar power only
        if (_verbose_logging) {
            MessageOutput.printf("%s%s: Consuming Solar Power Only -> adjustedVictronChargePower: %d, powerConsumption: %d\r\n", TAG, __FUNCTION__,
                adjustedVictronChargePower, newPowerLimit);
        }
        newPowerLimit = std::min(newPowerLimit, adjustedVictronChargePower);
    }

    if (_verbose_logging) {
        MessageOutput.printf("%s%s: newPowerLimit %d W\r\n", TAG, __FUNCTION__, newPowerLimit);
    }

    return newPowerLimit;
}

void PowerLimiterClass::commitPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t limit, bool enablePowerProduction)
{
    // disable power production as soon as possible.
    // setting the power limit is less important.
    if (!enablePowerProduction && inverter->isProducing()) {
        MessageOutput.printf("%s%s: Stopping inverter enablePowerProduction: %d, isProducing: %d ...\r\n", TAG, __FUNCTION__, enablePowerProduction, inverter->isProducing());
        inverter->sendPowerControlRequest(false);
    }

    inverter->sendActivePowerControlRequest(static_cast<float>(limit),
        PowerLimitControlType::AbsolutNonPersistent);

    _lastRequestedPowerLimit = limit;
    _lastPowerLimitMillis = millis();

    // enable power production only after setting the desired limit,
    // such that an older, greater limit will not cause power spikes.
    if (enablePowerProduction && !inverter->isProducing()) {
        MessageOutput.printf("%s%s: Starting up inverter...\r\n", TAG, __FUNCTION__);
        inverter->sendPowerControlRequest(true);
    }
}

/**
 * enforces limits and a hystersis on the requested power limit, after scaling
 * the power limit to the ratio of total and producing inverter channels.
 * commits the sanitized power limit. returns true if a limit update was
 * committed, false otherwise.
 */
bool PowerLimiterClass::setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newPowerLimit)
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    // Stop the inverter if limit is below threshold.
    if (newPowerLimit < cPL.LowerPowerLimit) {
        // the status must not change outside of loop(). this condition is
        // communicated through log messages already.
        if (_verbose_logging)
            MessageOutput.printf("%s%s: newPowerLimit %d < LowPowerLimit, %d\r\n", TAG, __FUNCTION__,
                newPowerLimit, cPL.LowerPowerLimit);
        return shutdown();
    }

    // enforce configured upper power limit
    int32_t effPowerLimit = std::min(newPowerLimit, cPL.UpperPowerLimit);

    // scale the power limit by the amount of all inverter channels devided by
    // the amount of producing inverter channels. the inverters limit each of
    // the n channels to 1/n of the total power limit. scaling the power limit
    // ensures the total inverter output is what we are asking for.
    std::list<ChannelNum_t> dcChnls = inverter->Statistics()->getChannelsByType(TYPE_DC);
    int dcProdChnls = 0, dcTotalChnls = dcChnls.size();
    for (auto& c : dcChnls) {
        if (inverter->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC) > 2.0) {
            dcProdChnls++;
        }
    }
    if ((dcProdChnls > 0) && (dcProdChnls != dcTotalChnls)) {
        if (_verbose_logging) {
            MessageOutput.printf("%s%s: %d channels total, %d producing channels, scaling power limit\r\n", TAG, __FUNCTION__, dcTotalChnls, dcProdChnls);
        }
        effPowerLimit = round(effPowerLimit * static_cast<float>(dcTotalChnls) / dcProdChnls);
    }

    effPowerLimit = std::min<int32_t>(effPowerLimit, inverter->DevInfo()->getMaxPower());

    // Check if the new value is within the limits of the hysteresis
    auto diff = std::abs(effPowerLimit - _lastRequestedPowerLimit);
    auto hysteresis = cPL.TargetPowerConsumptionHysteresis;

    // (re-)send power limit in case the last was sent a long time ago. avoids
    // staleness in case a power limit update was not received by the inverter.
    auto ageMillis = millis() - _lastPowerLimitMillis;

    if (diff < hysteresis && ageMillis < 60 * 1000) {
        if (_verbose_logging) {
            MessageOutput.printf("%s%s: requested: %d W, last limit: %d W, diff: %d W, hysteresis: %d W, age: %ld ms\r\n",
                TAG, __FUNCTION__,
                newPowerLimit, _lastRequestedPowerLimit, diff, hysteresis, ageMillis);
        }
        return false;
    }

    if (_verbose_logging) {
        MessageOutput.printf("%s%s: requested: %d W, (re-)sending limit: %d W\r\n",
            TAG, __FUNCTION__,
            newPowerLimit, effPowerLimit);
    }

    commitPowerLimit(inverter, effPowerLimit, true);
    return true;
}

int32_t PowerLimiterClass::getSolarChargePower()
{
    if (!canUseDirectSolarPower()) {
        return 0;
    }

    return VictronMppt.getPowerOutputWatts();
}

float PowerLimiterClass::getLoadCorrectedVoltage()
{
    if (!_inverter) {
        // there should be no need to call this method if no target inverter is known
        MessageOutput.println(F("DPL getLoadCorrectedVoltage: no inverter (programmer error)"));
        return 0.0;
    }

    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    float acPower = _inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
    if (_verbose_logging)
        MessageOutput.printf("%s%s: acPower %.1f\r\n", TAG, __FUNCTION__, acPower);
    float dcVoltage = getBatteryVoltage();

    if (dcVoltage <= 0.0) {
        return 0.0;
    }

    return dcVoltage + (acPower * cPL.VoltageLoadCorrectionFactor);
}

bool PowerLimiterClass::testThreshold(float socThreshold, float voltThreshold,
    std::function<bool(float, float)> compare)
{
    CONFIG_T& config = Configuration.get();

    // prefer SoC provided through battery interface, unless disabled by user
    auto stats = Battery.getStats();
    if (!config.PowerLimiter.IgnoreSoc
        && config.Battery.Enabled
        && socThreshold > 0.0
        && stats->isSoCValid()
        && stats->getSoCAgeSeconds() < 60) {
        return compare(stats->getSoC(), socThreshold);
    }

    // use voltage threshold as fallback
    if (voltThreshold <= 0.0) {
        return false;
    }

    return compare(getLoadCorrectedVoltage(), voltThreshold);
}

bool PowerLimiterClass::isStartThresholdReached()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    return testThreshold(
        cPL.BatterySocStartThreshold,
        cPL.VoltageStartThreshold,
        [](float a, float b) -> bool { return a >= b; });
}

bool PowerLimiterClass::isStopThresholdReached()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    return testThreshold(
        cPL.BatterySocStopThreshold,
        cPL.VoltageStopThreshold,
        [](float a, float b) -> bool { return a <= b; });
}

bool PowerLimiterClass::isBelowStopThreshold()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    return testThreshold(
        cPL.BatterySocStopThreshold,
        cPL.VoltageStopThreshold,
        [](float a, float b) -> bool { return a < b; });
}

/// @brief calculate next inverter restart in millis
void PowerLimiterClass::calcNextInverterRestart()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    // first check if restart is configured at all
    if (cPL.RestartHour < 0) {
        _nextInverterRestart = 1;
        MessageOutput.printf("%s%s: disabled\r\n", TAG, __FUNCTION__);
        return;
    }

    // read time from timeserver, if time is not synced then return
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5)) {
        // calculation first step is offset to next restart in minutes
        uint16_t dayMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        uint16_t targetMinutes = cPL.RestartHour * 60;
        if (cPL.RestartHour > timeinfo.tm_hour) {
            // next restart is on the same day
            _nextInverterRestart = targetMinutes - dayMinutes;
        } else {
            // next restart is on next day
            _nextInverterRestart = 1440 - dayMinutes + targetMinutes;
        }
        if (_verbose_logging) {
            MessageOutput.printf("%s%s: Localtime read %d %d / configured RestartHour %d\r\n", TAG, __FUNCTION__, timeinfo.tm_hour, timeinfo.tm_min, cPL.RestartHour);
            MessageOutput.printf("%s%s: dayMinutes %d / targetMinutes %d\r\n", TAG, __FUNCTION__, dayMinutes, targetMinutes);
            MessageOutput.printf("%s%s: next inverter restart in %d minutes\r\n", TAG, __FUNCTION__, _nextInverterRestart);
        }
        // then convert unit for next restart to milliseconds and add current uptime millis()
        _nextInverterRestart *= 60000;
        _nextInverterRestart += millis();
    } else {
        MessageOutput.printf("%s%s: getLocalTime not successful, no calculation\r\n", TAG, __FUNCTION__);
        _nextInverterRestart = 0;
    }
    MessageOutput.printf("%s%s: _nextInverterRestart @ %d millis\r\n", TAG, __FUNCTION__, _nextInverterRestart);
}

bool PowerLimiterClass::useFullSolarPassthrough()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    // We only do full solar PT if general solar PT is enabled
    if (!cPL.SolarPassThroughEnabled) {
        return false;
    }

    if (testThreshold(cPL.FullSolarPassThroughSoc,
            cPL.FullSolarPassThroughStartVoltage,
            [](float a, float b) -> bool { return a >= b; })) {
        _fullSolarPassThroughEnabled = true;
    }

    if (testThreshold(cPL.FullSolarPassThroughSoc,
            cPL.FullSolarPassThroughStopVoltage,
            [](float a, float b) -> bool { return a < b; })) {
        _fullSolarPassThroughEnabled = false;
    }

    return _fullSolarPassThroughEnabled;
}

bool PowerLimiterClass::manageBatteryDCpowerSwitch()
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    if (Configuration.get().Battery.Enabled // Battery must be enabled
        && !SunPosition.isAnnouceDayPeriod() //
        && _inverter->getEnablePolling() // Inverter connected to Battery must be enabled for polling
        && _inverter->getEnableCommands() // Inverter connected to Battery must be enabled to receive commands
        && Battery.getStats()->getSoC() >= cPL.BatterySocStopThreshold // Battery SoC must be geater than equal StopThreshold
        && MeanWellCan._rp.operation == 0) // only if Charger is off
    {
        switch (_preChargePowerState) {
        case 0:
            if ((millis() - _lastPreCharge > _preChargeDelay) && (Battery.getStats()->getSoC() >= cPL.BatterySocStartThreshold || (Battery.getStats()->getSoC() > cPL.BatterySocStopThreshold /*&& _lastDCState == false*/))) {
                MessageOutput.printf("%s%s: switch DC power of inverter id %d at channel %d OFF\r\n", TAG, __FUNCTION__, cPL.InverterId, cPL.InverterChannelId);
                digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
                digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
                _batteryDischargeEnabled = false;
                _inverter->setConnected(false);

                _lastPreCharge = millis();
                _preChargeDelay = 30 * 1000; // 30 seconds wait before start again
                _preChargePowerState = 1;
            }
            return false;

        case 1: // switch pre charge MosFET to load capacities of Inverter (soft power on)
            if (millis() - _lastPreCharge > _preChargeDelay) {
                MessageOutput.printf("%s%s: switch DC pre charging of inverter id %d at channel %d ON\r\n", TAG, __FUNCTION__, cPL.InverterId, cPL.InverterChannelId);
                digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
                digitalWrite(PinMapping.get().pre_charge, LOW); // MOSfet gate on

                _lastPreCharge = millis();
                _preChargeDelay = 5 * 1000; // 5 seconds pre charge
                _preChargePowerState = 2;
            }
            return false;

        case 2: // after pre charge time (5 seconds delay) switch the full power MosFET
            if (millis() - _lastPreCharge > _preChargeDelay) {
                MessageOutput.printf("%s%s: switch DC full power of inverter %d at channel %d ON\r\n", TAG, __FUNCTION__, cPL.InverterId, cPL.InverterChannelId);
                digitalWrite(PinMapping.get().full_power, LOW); // MOSfet gate on
                digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
                _inverter->setConnected(true);

                if (!isStopThresholdReached() && !isStartThresholdReached() /*&& cPL.Enabled != _lastDCState*/) {
                    if (_verbose_logging)
                        MessageOutput.printf("%s%s: initial condition (discharging allowed)\r\n", TAG, __FUNCTION__);
                    _batteryDischargeEnabled = true;
                    //                    _lastDCState = true;
                }

                _lastPreCharge = millis();
                _preChargeDelay = 60 * 1000; // give 60 seconds to start Inverter
                _preChargePowerState = 3;
            }
            return false;

        case 3:
            //                announceStatus(Status::WaitingInverterPowerOn);
            if (millis() - _lastPreCharge > _preChargeDelay) {
                _preChargeDelay = 5 * 1000; // at minimum 5 seconds delay
                _lastPreCharge = millis();
                _preChargePowerState = 4;
            }
            return false;

        case 4:
            break;
        }
    } else {
        shutdown(Status::DisconnectFromBattery);
        return false;
        /*
                // minimum time between start of inverter DC power and switch off DC power 30 seconds
                if ( (millis() - _lastPreCharge > 30 * 1000) && _preChargePowerState == 0) {
                    MessageOutput.printf("%s%s: switch DC power of inverter id %d at channel %d OFF\r\n", TAG, __FUNCTION__, cPL.InverterId, cPL.InverterChannelId);
                    digitalWrite(PinMapping.get().full_power, HIGH);    // MOSfet gate off
                    digitalWrite(PinMapping.get().pre_charge, HIGH);    // MOSfet gate off
                    _preChargeDelay= 30* 1000;
                    _lastPreCharge = millis();
                    _preChargePowerState = 0;
                    _inverter->setConnected(false);
                }
                if (_preChargePowerState == 0) return false;
        */
    }

    return true;
}
