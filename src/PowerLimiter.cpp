// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#include "Utils.h"
#include "Battery.h"
#include "PowerMeter.h"
#include "PowerLimiter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "Huawei_can.h"
#include "MeanWell_can.h"
#include <VictronMppt.h>
#include "MessageOutput.h"
#include "inverters/HMS_4CH.h"
#include "PinMapping.h"
#include "SunPosition.h"
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

    // to control MosFETs between battery and inverter
    // added by skippermeister
    _switchMosFetOffTimer=0;
    const PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;
    bool noDCpowerSwitch = PinMapping.get().pre_charge < 0 && PinMapping.get().full_power < 0;
    if (noDCpowerSwitch) {
        MessageOutput.printf("no MosFET pins configured to control DC power of inverter %" PRIx64 " channel %d\r\n",
            cPL.InverterId, cPL.InverterChannelId);
    } else {
        // switch PowerMOSFETs for inverter DC power off (battery powered Hoymiles inverter)
        MessageOutput.printf("switch DC power of inverter channel %d off. ", cPL.InverterChannelId);
        MessageOutput.printf("MosFET pins pre charge %d, full power %d, ", PinMapping.get().pre_charge, PinMapping.get().full_power);
        digitalWrite(PinMapping.get().pre_charge, HIGH);
        pinMode(PinMapping.get().pre_charge, OUTPUT);
        digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
        digitalWrite(PinMapping.get().full_power, HIGH);
        pinMode(PinMapping.get().full_power, OUTPUT);
        digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
        _preChargePowerState = 0;
        _lastPreCharge = millis();
        _preChargeDelay = 0;
    }

    if (cPL.InverterId > 0) {
        Hoymiles.getInverterBySerial(cPL.InverterId)->setConnected(noDCpowerSwitch);
        MessageOutput.println("done");
    } else {
        MessageOutput.println("no Inverter for Powerlimiter configured");
    }
}

frozen::string const& PowerLimiterClass::getStatusText(PowerLimiterClass::Status status)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Status, frozen::string, 22> texts = {
        { Status::Initializing, "initializing (should not see me)" },
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::DisabledByMqtt, "disabled by MQTT" },
        { Status::WaitingForValidTimestamp, "waiting for valid date and time to be available" },
        { Status::PowerMeterPending, "waiting for sufficiently recent power meter reading" },
        { Status::InverterInvalid, "invalid inverter selection/configuration" },
        { Status::InverterChanged, "target inverter changed" },
        { Status::InverterOffline, "inverter is offline (polling enabled? radio okay?)" },
        { Status::InverterCommandsDisabled, "inverter configuration prohibits sending commands" },
        { Status::InverterLimitPending, "waiting for a power limit command to complete" },
        { Status::InverterPowerCmdPending, "waiting for a start/stop/restart command to complete" },
        { Status::InverterDevInfoPending, "waiting for inverter device information to be available" },
        { Status::InverterStatsPending, "waiting for sufficiently recent inverter data" },
        { Status::CalculatedLimitBelowMinLimit, "calculated limit is less than minimum power limit" },
        { Status::UnconditionalSolarPassthrough, "unconditionally passing through all solar power (MQTT override)" },
        { Status::NoVeDirect, "VE.Direct disabled, connection broken, or data outdated" },
        { Status::NoEnergy, "no energy source available to power the inverter from" },
        { Status::MeanWellPsu, "DPL stands by while MeanWell PSU is enabled/charging" },
        { Status::Stable, "the system is stable, the last power limit is still valid" },
        { Status::TemperatureRange, "temperatue out of recommended discharge range (-10°C ~ 50°C)" },
        { Status::BatteryNotInitialized, "battery is not initialized" },
        { Status::DisconnectFromBattery, "disconnect inverter from battery" }
        //        { Status::WaitingInverterPowerOn, "waiting till Inverter is startet (DC power on)"}
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}

void PowerLimiterClass::announceStatus(PowerLimiterClass::Status status)
{
    // this method is called with high frequency. print the status text if
    // the status changed since we last printed the text of another one.
    // otherwise repeat the info with a fixed interval.
    if (_lastStatus == status && !_lastStatusPrinted.occured()) { return; }

    // after announcing once that the DPL is disabled by configuration, it
    // should just be silent while it is disabled.
    if (status == Status::DisabledByConfig && _lastStatus == status) { return; }

    MessageOutput.printf("%s%s: %s\r\n", TAG, __FUNCTION__, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted.reset();
}

/**
 * returns true if the inverter state was changed or is about to change, i.e.,
 * if it is actually in need of a shutdown. returns false otherwise, i.e., the
 * inverter is already shut down.
 */
bool PowerLimiterClass::shutdown(PowerLimiterClass::Status status)
{
    // added by skippermeister
    if (_lastStatus != Status::DisconnectFromBattery)
        announceStatus(status);

    _shutdownPending = true;

    _oTargetPowerState = false;

    // needed if MosFETs to control DC power of inverter between battery and inverter
    // added by skippermeister
    _switchMosFetOffTimer = millis();

    return updateInverter();
}

void PowerLimiterClass::loop()
{
    auto const& config = Configuration.get();
    _verboseLogging = config.PowerLimiter.VerboseLogging;

    // we know that the Hoymiles library refuses to send any message to any
    // inverter until the system has valid time information. until then we can
    // do nothing, not even shutdown the inverter.
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5)) {
        return announceStatus(Status::WaitingForValidTimestamp);
    }

    // take care that the last requested power
    // limit and power state are actually reached
    if (updateInverter()) { return; }

    if (_shutdownPending) {
        // controll MosFets between battery and inverter
        // added by skippermeister
        if (PinMapping.get().pre_charge >= 0 && PinMapping.get().full_power >= 0 &&
            _switchMosFetOffTimer) {
            // wait till timer elapsed or inverter stops
            if ( millis() - _switchMosFetOffTimer > 60*1000 ) {
                switchMosFetsOff();
            } else {
                return;
            }
        }
        _shutdownPending = false;
        _inverter = nullptr;
    }

    if (!config.PowerLimiter.Enabled) {
        shutdown(Status::DisabledByConfig);
        return;
    }

    if (Mode::Disabled == _mode) {
        shutdown(Status::DisabledByMqtt);
        return;
    }

    // wait till battery provider is initialized and battery is fully scanned
    // added by skippermeister
    if (!Battery.initialized()) {
        shutdown(Status::BatteryNotInitialized);
        return;
    }

    // check if battery is in allowed temperature range
    // added by skippermeister
    if (!Battery.getStats()->isDischargeTemperatureValid()) {
        shutdown(Status::TemperatureRange);
        return;
    }

    std::shared_ptr<InverterAbstract> currentInverter = Hoymiles.getInverterBySerial(config.PowerLimiter.InverterId);

/*
    // always done in module read configuration file
    // removed by skippermeister
    if (currentInverter == nullptr && config.PowerLimiter.InverterId < INV_MAX_COUNT) {
        // we previously had an index saved as InverterId. fall back to the
        // respective positional lookup if InverterId is not a known serial.
        currentInverter = Hoymiles.getInverterByPos(config.PowerLimiter.InverterId);
    }
*/
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

    // manage MosFets between batterie and inverter
    // added by skippermeister
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

    // concerns both power limits and start/stop/restart commands and is
    // only updated if a respective response was received from the inverter
    auto lastUpdateCmd = std::max(
        _inverter->SystemConfigPara()->getLastUpdateCommand(),
        _inverter->PowerCommand()->getLastUpdateCommand());

    // we need inverter stats younger than the last update command
    if (_oInverterStatsMillis.has_value() && lastUpdateCmd > *_oInverterStatsMillis) {
        _oInverterStatsMillis = std::nullopt;
    }

    if (!_oInverterStatsMillis.has_value()) {
        auto lastStats = _inverter->Statistics()->getLastUpdate();
        if (lastStats <= lastUpdateCmd) {
            return announceStatus(Status::InverterStatsPending);
        }

        _oInverterStatsMillis = lastStats;
    }

    // if the power meter is being used, i.e., if its data is valid, we want to
    // wait for a new reading after adjusting the inverter limit. otherwise, we
    // proceed as we will use a fallback limit independent of the power meter.
    // the power meter reading is expected to be at most 2 seconds old when it
    // arrives. this can be the case for readings provided by networked meter
    // readers, where a packet needs to travel through the network for some
    // time after the actual measurement was done by the reader.
    if (PowerMeter.isDataValid() && (PowerMeter.getLastUpdate() - *_oInverterStatsMillis) <= 2000) {
        return announceStatus(Status::PowerMeterPending);
    }

    // since _lastCalculation and _calculationBackoffMs are initialized to
    // zero, this test is passed the first time the condition is checked.
    if (millis() - _lastCalculation < _calculationBackoffMs) {
        return announceStatus(Status::Stable);
    }

    if (_verboseLogging) MessageOutput.printf("%s ******************* ENTER **********************\r\n", TAG);

    // Check if next inverter restart time is reached
    if ((_nextInverterRestart > 1) && (millis() - _nextInverterRestart > 0)) {
        MessageOutput.printf("%s%s: send inverter restart\r\n", TAG, __FUNCTION__);
        _inverter->sendRestartControlRequest();
        calcNextInverterRestart();
    }

    // Check if NTP time is set and next inverter restart not calculated yet
    if ((config.PowerLimiter.RestartHour >= 0) && (_nextInverterRestart == 0)) {
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

    auto getBatteryPower = [this,&config]() -> bool {
        if (config.PowerLimiter.IsInverterSolarPowered) { return false; }

        if (_nighttimeDischarging && getSolarPower() > 0) {
            _nighttimeDischarging = false;
            return isStartThresholdReached();
        }

        if (isStopThresholdReached()) {
            if (_verboseLogging) MessageOutput.printf("%s%s: Stop threshold reached (discharging not allowed)\r\n", TAG, __FUNCTION__);
            return false;
        }

        if (isStartThresholdReached()) {
            if (_verboseLogging) MessageOutput.printf("%s%s: Start threshold reached (discharging allowed)\r\n", TAG, __FUNCTION__);
            return true;
        }

        // TODO(schlimmchen): should be supported by sunrise and sunset, such
        // that a thunderstorm or other events that drastically lower the solar
        // power do not cause the start of a discharge cycle during the day.
        if (config.PowerLimiter.SolarPassThroughEnabled &&
            config.PowerLimiter.BatteryAlwaysUseAtNight &&
            getSolarPower() == 0 && !_batteryDischargeEnabled) {
            _nighttimeDischarging = true;
            if (_verboseLogging) MessageOutput.printf("%s%s: Solar passthroug is enabled and use battery always at night\r\n", TAG, __FUNCTION__);
            return true;
        }

        // we are between start and stop threshold and keep the state that was
        // last triggered, either charging or discharging.
        return _batteryDischargeEnabled;
    };

    _batteryDischargeEnabled = getBatteryPower();

    if (_verboseLogging && !config.PowerLimiter.IsInverterSolarPowered) {
        MessageOutput.printf("%s%s: battery interface %sabled, SoC: %.1f%%, StartTH: %d %%, StopTH: %d %%, SoC age: %d s, ignore: %s\r\n", TAG, __FUNCTION__,
            (Configuration.get().Battery.Enabled ? "en" : "dis"),
            Battery.getStats()->getSoC(),
            config.PowerLimiter.BatterySocStartThreshold,
            config.PowerLimiter.BatterySocStopThreshold,
            Battery.getStats()->getSoCAgeSeconds(),
            config.PowerLimiter.IgnoreSoc ? "yes" : "no");

        auto dcVoltage = getBatteryVoltage(true /*log voltages only once per DPL loop*/);
        MessageOutput.printf("%s%s: dcVoltage: %.2f V, loadCorrectedVoltage: %.2f V, StartTH: %.2f V, StopTH: %.2f V\r\n", TAG, __FUNCTION__,
            dcVoltage, getLoadCorrectedVoltage(),
            config.PowerLimiter.VoltageStartThreshold,
            config.PowerLimiter.VoltageStopThreshold);

        MessageOutput.printf("%s%s: StartTH reached: %s, StopTH reached: %s, SolarPT %sabled, use at night: %s\r\n", TAG, __FUNCTION__,
            (isStartThresholdReached() ? "yes" : "no"),
            (isStopThresholdReached() ? "yes" : "no"),
            (config.PowerLimiter.SolarPassThroughEnabled ? "en" : "dis"),
            (config.PowerLimiter.BatteryAlwaysUseAtNight?"yes":"no"));
    }

    // Calculate and set Power Limit (NOTE: might reset _inverter to nullptr!)
    bool limitUpdated = calcPowerLimit(_inverter, getSolarPower(), _batteryDischargeEnabled);

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
        MessageOutput.println("[DPL::getBatteryVoltage] no inverter (programmer error)");
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

    if (log) MessageOutput.printf("%s%s: BMS: %.2f V, MPPT: %.2f V, inverter: %.2f V, returning: %.2fV\r\n", TAG, __FUNCTION__,
            bmsVoltage, chargeControllerVoltage, inverterVoltage, res);

    return res;
}

static float getInverterEfficiency(std::shared_ptr<InverterAbstract> inverter)
{
    float inverterEfficiencyPercent = inverter->Statistics()->getChannelFieldValue(
        TYPE_INV, CH0, FLD_EFF);

    // fall back to hoymiles peak efficiency as per datasheet if inverter
    // is currently not producing (efficiency is zero in that case)
    return  (inverterEfficiencyPercent > 0) ? inverterEfficiencyPercent/100 : 0.967;
}

/**
 * calculate the AC output power (limit) to set, such that the inverter uses
 * the given power on its DC side, i.e., adjust the power for the inverter's
 * efficiency.
 */
int32_t PowerLimiterClass::inverterPowerDcToAc(std::shared_ptr<InverterAbstract> inverter, int32_t dcPower)
{
    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    float inverterEfficiencyFactor = getInverterEfficiency(inverter);

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
    if ((millis() - _lastCalculation) < _calculationBackoffMs) { return; }
    _lastCalculation = millis();

    auto const& config = Configuration.get();

    if (config.PowerLimiter.IsInverterSolarPowered) {
        _calculationBackoffMs = 10 * 1000;
        setNewPowerLimit(inverter, config.PowerLimiter.UpperPowerLimit);
        announceStatus(Status::UnconditionalSolarPassthrough);
        return;
    }

    if (!VictronMppt.isDataValid()) {
        shutdown(Status::NoVeDirect);
        return;
    }

    _calculationBackoffMs = 1 * 1000;
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

// Logic table ("PowerMeter value" can be "base load setting" as a fallback)
// | Case # | batteryPower | solarPower     | useFullSolarPassthrough | Resulting inverter limit                               |
// | 1      | false        |  < 20 W        | doesn't matter          | 0 (inverter off)                                       |
// | 2      | false        | >= 20 W        | doesn't matter          | min(PowerMeter value, solarPower)                      |
// | 3      | true         | doesn't matter | false                   | PowerMeter value (Battery can supply unlimited energy) |
// | 4      | true         | fully passed   | true                    | max(PowerMeter value, solarPower)                      |

bool PowerLimiterClass::calcPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t solarPowerDC, bool batteryPower)
{
    if (_verboseLogging) MessageOutput.printf("%s%s: battery use %s, solar power (DC): %d W\r\n", TAG, __FUNCTION__,
                (batteryPower?"allowed":"prevented"), solarPowerDC);

    // Case 1:
    if (solarPowerDC <= 0 && !batteryPower) {
        return shutdown(Status::NoEnergy);
    }

    // We check if the PSU is on and disable the Power Limiter in this case.
    // The PSU should reduce power or shut down first before the Power Limiter
    // kicks in. The only case where this is not desired is if the battery is
    // over the Full Solar Passthrough Threshold. In this case the Power
    // Limiter should run and the PSU will shut down as a consequence.
    if (!useFullSolarPassthrough() &&
#ifdef CHARGER_HUAWEI
        HuaweiCan.getAutoPowerStatus()
#else
        MeanWellCan.getAutoPowerStatus()
#endif
    ) {
        return shutdown(Status::MeanWellPsu);
    }

    auto meterValid = PowerMeter.isDataValid();

    auto meterValue = static_cast<int32_t>(PowerMeter.getPowerTotal());

    // We don't use FLD_PAC from the statistics, because that data might be too
    // old and unreliable. TODO(schlimmchen): is this comment outdated?
    auto inverterOutput = static_cast<int32_t>(inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));

    auto solarPowerAC = inverterPowerDcToAc(inverter, solarPowerDC);

    auto const& config = Configuration.get();
    auto targetConsumption = config.PowerLimiter.TargetPowerConsumption;
    auto baseLoad = config.PowerLimiter.BaseLoadLimit;
    bool meterIncludesInv = config.PowerLimiter.IsInverterBehindPowerMeter;

    if (_verboseLogging) {
        MessageOutput.printf("%s%s: target consumption: %d W, "
                "base load: %d W, power meter does %sinclude inverter output\r\n",
                TAG, __FUNCTION__,
                targetConsumption,
                baseLoad,
                (meterIncludesInv?"":"NOT "));

        MessageOutput.printf("%s%s: power meter: %d W, power meter valid: %s, inverter output: %d W, solar power (AC): %d\r\n",
            TAG, __FUNCTION__,
            meterValue,
            (meterValid?"yes":"no"),
            inverterOutput,
            solarPowerAC);
    }

    auto newPowerLimit = baseLoad;

    if (meterValid) {
        newPowerLimit = meterValue;

        if (meterIncludesInv) {
            // If the inverter is wired behind the power meter, i.e., if its
            // output is part of the power meter measurement, the produced
            // power of this inverter has to be taken into account.
            newPowerLimit += inverterOutput;
        }

        newPowerLimit -= targetConsumption;
    }

    // Case 2:
    if (!batteryPower) {
        newPowerLimit = std::min(newPowerLimit, solarPowerAC);

        // do not drain the battery. use as much power as needed to match the
        // household consumption, but not more than the available solar power.
        if (_verboseLogging) MessageOutput.printf("%s%s: limited to solar power: %d W\r\n", TAG, __FUNCTION__, newPowerLimit);

        return setNewPowerLimit(inverter, newPowerLimit);
    }


    // Case 4:
    // convert all solar power if full solar-passthrough is active
    if (useFullSolarPassthrough()) {
        newPowerLimit = std::max(newPowerLimit, solarPowerAC);

        if (_verboseLogging) MessageOutput.printf("%s%s: full solar-passthrough active: %d W\r\n", TAG, __FUNCTION__, newPowerLimit);

        return setNewPowerLimit(inverter, newPowerLimit);
    }

    if (_verboseLogging) MessageOutput.printf("%s%s: match household consumption with limit of %d W\r\n", TAG, __FUNCTION__, newPowerLimit);

    // Case 3:
    return setNewPowerLimit(inverter, newPowerLimit);
}

/**
 * updates the inverter state (power production and limit). returns true if a
 * change to its state was requested or is pending. this function only requests
 * one change (limit value or production on/off) at a time.
 */
bool PowerLimiterClass::updateInverter()
{
    auto reset = [this]() -> bool {
        _oTargetPowerState = std::nullopt;
        _oTargetPowerLimitWatts = std::nullopt;
        _oUpdateStartMillis = std::nullopt;
        return false;
    };

    if (nullptr == _inverter) { return reset(); }

    // do not reset _inverterUpdateTimeouts below if no state change requested
    if (!_oTargetPowerState.has_value() && !_oTargetPowerLimitWatts.has_value()) {
        return reset();
    }

    if (!_oUpdateStartMillis.has_value()) { _oUpdateStartMillis = millis(); }

    if ((millis() - *_oUpdateStartMillis) > 30 * 1000) {
        ++_inverterUpdateTimeouts;
        if (_verboseLogging) MessageOutput.printf("%s%s: timeout (%d in succession), state transition pending: %s, limit pending: %s\r\n", TAG, __FUNCTION__,
            _inverterUpdateTimeouts,
            (_oTargetPowerState.has_value()?"yes":"no"),
            (_oTargetPowerLimitWatts.has_value()?"yes":"no"));

        // NOTE that this is not always 5 minutes, since this counts timeouts,
        // not absolute time. after any timeout, an update cycle ends. a new
        // timeout can only happen after starting a new update cycle, which in
        // turn is only started if the DPL did calculate a new limit, which in
        // turn does not happen while the inverter is unreachable, no matter
        // how long (a whole night) that might be.
        if (_inverterUpdateTimeouts >= 10) {
            MessageOutput.printf("%s%s: issuing inverter restart command after update timed out repeatedly\r\n", TAG, __FUNCTION__);
            _inverter->sendRestartControlRequest();
        }

        if (_inverterUpdateTimeouts >= 20) {
            MessageOutput.printf("%s%s: restarting system since inverter is unresponsive\r\n", TAG, __FUNCTION__);
            Utils::restartDtu();
        }

        return reset();
    }

    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;

    auto switchPowerState = [this](bool transitionOn) -> bool {
        // no power state transition requested at all
        if (!_oTargetPowerState.has_value()) { return false; }

        // the transition that may be started is not the one which is requested
        if (transitionOn != *_oTargetPowerState) { return false; }

        // wait for pending power command(s) to complete
        auto lastPowerCommandState = _inverter->PowerCommand()->getLastPowerCommandSuccess();
        if (CMD_PENDING == lastPowerCommandState) {
            announceStatus(Status::InverterPowerCmdPending);
            return true;
        }

        // we need to wait for statistics that are more recent than the last
        // power update command to reliably use _inverter->isProducing()
        auto lastPowerCommandMillis = _inverter->PowerCommand()->getLastUpdateCommand();
        auto lastStatisticsMillis = _inverter->Statistics()->getLastUpdate();
        if ((lastStatisticsMillis - lastPowerCommandMillis) > halfOfAllMillis) { return true; }

        if (_inverter->isProducing() != *_oTargetPowerState) {
            MessageOutput.printf("%s%s: %s inverter...\r\n", TAG, __FUNCTION__, ((*_oTargetPowerState)?"Starting":"Stopping"));
            _inverter->sendPowerControlRequest(*_oTargetPowerState);
            return true;
        }

        _oTargetPowerState = std::nullopt; // target power state reached
        return false;
    };

    // we use a lambda function here to be able to use return statements,
    // which allows to avoid if-else-indentions and improves code readability
    auto updateLimit = [this]() -> bool {
        // no limit update requested at all
        if (!_oTargetPowerLimitWatts.has_value()) { return false; }

        // wait for pending limit command(s) to complete
        auto lastLimitCommandState = _inverter->SystemConfigPara()->getLastLimitCommandSuccess();
        if (CMD_PENDING == lastLimitCommandState) {
            announceStatus(Status::InverterLimitPending);
            return true;
        }

        auto maxPower = _inverter->DevInfo()->getMaxPower();
        auto newRelativeLimit = static_cast<float>(*_oTargetPowerLimitWatts * 100) / maxPower;

        // if no limit command is pending, the SystemConfigPara does report the
        // current limit, as the answer by the inverter to a limit command is
        // the canonical source that updates the known current limit.
        auto currentRelativeLimit = _inverter->SystemConfigPara()->getLimitPercent();

        // we assume having exclusive control over the inverter. if the last
        // limit command was successful and sent after we started the last
        // update cycle, we should assume *our* requested limit was set.
        uint32_t lastLimitCommandMillis = _inverter->SystemConfigPara()->getLastUpdateCommand();
        if ((lastLimitCommandMillis - *_oUpdateStartMillis) < halfOfAllMillis && CMD_OK == lastLimitCommandState) {
            if (_verboseLogging) MessageOutput.printf("%s%s: actual limit is %.1f %% (%.0f W respectively), effective %d ms after update started, requested were %.1f %%\r\n",
                TAG, __FUNCTION__,
                currentRelativeLimit,
                (currentRelativeLimit * maxPower / 100),
                (lastLimitCommandMillis - *_oUpdateStartMillis),
                newRelativeLimit);

            if (std::abs(newRelativeLimit - currentRelativeLimit) > 2.0) {
                if (_verboseLogging) MessageOutput.printf("%s%s: NOTE: expected limit of %.1f %% and actual limit of %.1f %% mismatch by more than 2 %%, "
                                     "is the DPL in exclusive control over the inverter?\r\n",
                                        TAG, __FUNCTION__,
                                        newRelativeLimit, currentRelativeLimit);
            }

            _oTargetPowerLimitWatts = std::nullopt;
            return false;
        }

        if (_verboseLogging) MessageOutput.printf("%s%s: sending limit of %.1f %% (%.0f W respectively), max output is %d W\r\n", TAG, __FUNCTION__,
                newRelativeLimit, (newRelativeLimit * maxPower / 100), maxPower);

        _inverter->sendActivePowerControlRequest(static_cast<float>(newRelativeLimit), PowerLimitControlType::RelativNonPersistent);

        _lastRequestedPowerLimit = *_oTargetPowerLimitWatts;
        return true;
    };

    // disable power production as soon as possible.
    // setting the power limit is less important once the inverter is off.
    if (switchPowerState(false)) { return true; }

    if (updateLimit()) { return true; }

    // enable power production only after setting the desired limit
    if (switchPowerState(true)) { return true; }

    _inverterUpdateTimeouts = 0;

    return reset();
}

/**
 * scale the desired inverter limit such that the actual inverter AC output is
 * close to the desired power limit, even if some input channels are producing
 * less than the limit allows. this happens because the inverter seems to split
 * the total power limit equally among all MPPTs (not inputs; some inputs share
 * the same MPPT on some models).
 *
 * TODO(schlimmchen): the current implementation is broken and is in need of
 * refactoring. currently it only works for inverters that provide one MPPT for
 * each input.
 */
int32_t PowerLimiterClass::scalePowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newLimit, int32_t currentLimitWatts)
{
    // prevent scaling if inverter is not producing, as input channels are not
    // producing energy and hence are detected as not-producing, causing
    // unreasonable scaling.
    if (!inverter->isProducing()) { return newLimit; }

    std::list<ChannelNum_t> dcChnls = inverter->Statistics()->getChannelsByType(TYPE_DC);
    size_t dcTotalChnls = dcChnls.size();

    // according to the upstream projects README (table with supported devs),
    // every 2 channel inverter has 2 MPPTs. then there are the HM*S* 4 channel
    // models which have 4 MPPTs. all others have a different number of MPPTs
    // than inputs. those are not supported by the current scaling mechanism.
    bool supported = dcTotalChnls == 2;
#ifdef USE_RADIO_CMT
    supported |= dcTotalChnls == 4 && HMS_4CH::isValidSerial(inverter->serial());
#endif
    if (!supported) { return newLimit; }

    // test for a reasonable power limit that allows us to assume that an input
    // channel with little energy is actually not producing, rather than
    // producing very little due to the very low limit.
    if (currentLimitWatts < dcTotalChnls * 10) { return newLimit; }

    auto const& config = Configuration.get();
    auto allowOverscaling = config.PowerLimiter.UseOverscalingToCompensateShading;
    auto isInverterSolarPowered = config.PowerLimiter.IsInverterSolarPowered;

    // overscalling allows us to compensate for shaded panels by increasing the
    // total power limit, if the inverter is solar powered.
    if (allowOverscaling && isInverterSolarPowered) {
        auto inverterOutputAC = inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
        float inverterEfficiencyFactor = getInverterEfficiency(inverter);

        // 98% of the expected power is good enough
        auto expectedAcPowerPerChannel = (currentLimitWatts / dcTotalChnls) * 0.98;

        if (_verboseLogging) MessageOutput.printf("%s%s: expected AC power per channel %f W\r\n",
                    TAG, __FUNCTION__, expectedAcPowerPerChannel);

        size_t dcShadedChnls = 0;
        auto shadedChannelACPowerSum = 0.0;

        for (auto& c : dcChnls) {
            auto channelPowerAC = inverter->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC) * inverterEfficiencyFactor;

            if (channelPowerAC < expectedAcPowerPerChannel) {
                dcShadedChnls++;
                shadedChannelACPowerSum += channelPowerAC;
            }

            if (_verboseLogging) MessageOutput.printf("%s%s: ch %d AC power %f W\r\n",
                TAG, __FUNCTION__, c, channelPowerAC);
        }

        // no shading or the shaded channels provide more power than what
        // we currently need.
        if (dcShadedChnls == 0 || shadedChannelACPowerSum >= newLimit) { return newLimit; }

        if (dcShadedChnls == dcTotalChnls) {
            // keep the currentLimit when:
            // - all channels are shaded
            // - currentLimit >= newLimit
            // - we get the expected AC power or less and
            if (currentLimitWatts >= newLimit && inverterOutputAC <= newLimit) {
                if (_verboseLogging) MessageOutput.printf("%s%s: all channels are shaded, keeping the current limit of %d W\r\n",
                    TAG, __FUNCTION__, currentLimitWatts);

                return currentLimitWatts;

            } else {
                return newLimit;
            }
        }

        size_t dcNonShadedChnls = dcTotalChnls - dcShadedChnls;
        auto overScaledLimit = static_cast<int32_t>((newLimit - shadedChannelACPowerSum) / dcNonShadedChnls * dcTotalChnls);

        if (overScaledLimit <= newLimit) { return newLimit; }

        if (_verboseLogging) MessageOutput.printf("%s%s: %d/%d channels are shaded, scaling %d W\r\n",
            TAG, __FUNCTION__, dcShadedChnls, dcTotalChnls, overScaledLimit);

        return overScaledLimit;
    }

    size_t dcProdChnls = 0;
    for (auto& c : dcChnls) {
        if (inverter->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC) > 2.0) {
            dcProdChnls++;
        }
    }

    if (dcProdChnls == 0 || dcProdChnls == dcTotalChnls) { return newLimit; }

    auto scaled = static_cast<int32_t>(newLimit * static_cast<float>(dcTotalChnls) / dcProdChnls);
    if (_verboseLogging) MessageOutput.printf("%s%s: %d/%d channels are producing, scaling from %d to %d W\r\n",
        TAG, __FUNCTION__, dcProdChnls, dcTotalChnls, newLimit, scaled);
    return scaled;
}

/**
 * enforces limits on the requested power limit, after scaling the power limit
 * to the ratio of total and producing inverter channels. commits the sanitized
 * power limit. returns true if an inverter update was committed, false
 * otherwise.
 */
bool PowerLimiterClass::setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newPowerLimit)
{
    auto const& config = Configuration.get();
    auto lowerLimit = config.PowerLimiter.LowerPowerLimit;
    auto upperLimit = config.PowerLimiter.UpperPowerLimit;
    auto hysteresis = config.PowerLimiter.TargetPowerConsumptionHysteresis;

    if (_verboseLogging) MessageOutput.printf("%s%s: input limit: %d W, min limit: %d W, max limit: %d W, hysteresis: %d W\r\n",
            TAG, __FUNCTION__,
            newPowerLimit, lowerLimit, upperLimit, hysteresis);

    if (newPowerLimit < lowerLimit) {
        if (!config.PowerLimiter.IsInverterSolarPowered) {
            return shutdown(Status::CalculatedLimitBelowMinLimit);
        }

        MessageOutput.printf("%s%s: keep solar-powered inverter running at min limit\r\n", TAG, __FUNCTION__);
        newPowerLimit = lowerLimit;
    }

    // enforce configured upper power limit
    int32_t effPowerLimit = std::min(newPowerLimit, upperLimit);

    // early in the loop we make it a pre-requisite that this
    // value is non-zero, so we can assume it to be valid.
    auto maxPower = inverter->DevInfo()->getMaxPower();

    float currentLimitPercent = inverter->SystemConfigPara()->getLimitPercent();
    auto currentLimitAbs = static_cast<int32_t>(currentLimitPercent * maxPower / 100);

    effPowerLimit = scalePowerLimit(inverter, effPowerLimit, currentLimitAbs);

    effPowerLimit = std::min<int32_t>(effPowerLimit, maxPower);

    auto diff = std::abs(currentLimitAbs - effPowerLimit);

    if (_verboseLogging) MessageOutput.printf("%s%s: inverter max: %d W, inverter %s producing, requesting: %d W, reported: %d W, diff: %d W\r\n",
            TAG, __FUNCTION__,
            maxPower, (inverter->isProducing()?"is":"is NOT"),
            effPowerLimit, currentLimitAbs, diff);

    if (diff > hysteresis) {
        _oTargetPowerLimitWatts = effPowerLimit;
    }

    _oTargetPowerState = true;
    return updateInverter();
}

int32_t PowerLimiterClass::getSolarPower()
{
    auto const& config = Configuration.get();

    if (config.PowerLimiter.IsInverterSolarPowered) {
        // the returned value is arbitrary, as long as it's
        // greater than the inverters max DC power consumption.
        return 10 * 1000;
    }

    if (!config.PowerLimiter.SolarPassThroughEnabled
            || isBelowStopThreshold()
            || !VictronMppt.isDataValid()) {
        return 0;
    }

    auto solarPower = VictronMppt.getPowerOutputWatts();
    if (solarPower < 20) { return 0; } // too little to work with

    return solarPower;
}

float PowerLimiterClass::getLoadCorrectedVoltage()
{
    if (!_inverter) {
        // there should be no need to call this method if no target inverter is known
        MessageOutput.printf("%s%s: no inverter (programmer error)\r\n", TAG, __FUNCTION__);
        return 0.0;
    }

    CONFIG_T& config = Configuration.get();

    float acPower = _inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
    if (_verboseLogging) MessageOutput.printf("%s%s: acPower %.1f\r\n", TAG, __FUNCTION__, acPower);
    float dcVoltage = getBatteryVoltage();

    if (dcVoltage <= 0.0) {
        return 0.0;
    }

    return dcVoltage + (acPower * config.PowerLimiter.VoltageLoadCorrectionFactor);
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
    if (voltThreshold <= 0.0) { return false; }

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
    CONFIG_T& config = Configuration.get();

    // first check if restart is configured at all
    if (config.PowerLimiter.RestartHour < 0) {
        _nextInverterRestart = 1;
        MessageOutput.printf("%s%s: _nextInverterRestart disabled\r\n", TAG, __FUNCTION__);
        return;
    }

    if (config.PowerLimiter.IsInverterSolarPowered) {
        _nextInverterRestart = 1;
        MessageOutput.printf("%s%s: not restarting solar-powered inverters\r\n", TAG, __FUNCTION__);
        return;
    }

    // read time from timeserver, if time is not synced then return
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5)) {
        // calculation first step is offset to next restart in minutes
        uint16_t dayMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        uint16_t targetMinutes = config.PowerLimiter.RestartHour * 60;
        if (config.PowerLimiter.RestartHour > timeinfo.tm_hour) {
            // next restart is on the same day
            _nextInverterRestart = targetMinutes - dayMinutes;
        } else {
            // next restart is on next day
            _nextInverterRestart = 1440 - dayMinutes + targetMinutes;
        }
        if (_verboseLogging) {
            MessageOutput.printf("%s%s: Localtime read %d %d / configured RestartHour %d\r\n", TAG, __FUNCTION__, timeinfo.tm_hour, timeinfo.tm_min, config.PowerLimiter.RestartHour);
            MessageOutput.printf("%s%s: dayMinutes %d / targetMinutes %d\r\n", TAG, __FUNCTION__, dayMinutes, targetMinutes);
            MessageOutput.printf("%s%s: next inverter restart in %d minutes\r\n", TAG, __FUNCTION__, _nextInverterRestart);
        }
        // then convert unit for next restart to milliseconds and add current uptime millis()
        _nextInverterRestart *= 60000;
        _nextInverterRestart += millis();
    } else {
        if (_verboseLogging) MessageOutput.printf("%s%s: getLocalTime not successful, no calculation\r\n", TAG, __FUNCTION__);
        _nextInverterRestart = 0;
    }
    if (_verboseLogging) MessageOutput.printf("%s%s: _nextInverterRestart @ %d millis\r\n", TAG, __FUNCTION__, _nextInverterRestart);
}

bool PowerLimiterClass::useFullSolarPassthrough()
{
    auto const& config = Configuration.get();

    // solar passthrough only applies to setups with battery-powered inverters
    if (config.PowerLimiter.IsInverterSolarPowered) { return false; }

    // We only do full solar PT if general solar PT is enabled
    if (!config.PowerLimiter.SolarPassThroughEnabled) { return false; }

    if (testThreshold(config.PowerLimiter.FullSolarPassThroughSoc,
            config.PowerLimiter.FullSolarPassThroughStartVoltage,
            [](float a, float b) -> bool { return a >= b; })) {
        _fullSolarPassThroughEnabled = true;
    }

    if (testThreshold(config.PowerLimiter.FullSolarPassThroughSoc,
            config.PowerLimiter.FullSolarPassThroughStopVoltage,
            [](float a, float b) -> bool { return a < b; })) {
        _fullSolarPassThroughEnabled = false;
    }

    return _fullSolarPassThroughEnabled;
}

bool PowerLimiterClass::manageBatteryDCpowerSwitch()
{
    // skip algorithm if we have not implemented the MosFETs between battery and inverter
    if (PinMapping.get().pre_charge < 0 && PinMapping.get().full_power < 0) return true;

    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    if (Configuration.get().Battery.Enabled // Battery must be enabled
        && !SunPosition.isAnnouceDayPeriod() //
        && _inverter->getEnablePolling() // Inverter connected to Battery must be enabled for polling
        && _inverter->getEnableCommands() // Inverter connected to Battery must be enabled to receive commands
        && !isStopThresholdReached() // Battery SoC must be geater than equal StopThreshold
        && MeanWellCan._rp.operation == 0) // only if Charger is off
    {
        switch (_preChargePowerState) {
        case 0:
            if ((millis() - _lastPreCharge > _preChargeDelay) && (isStartThresholdReached() /*|| (!isStopThresholdReached() && _lastDCState == false)*/ )) {
                MessageOutput.printf("%s%s: switch DC power of inverter id %" PRIx64 " at channel %d OFF\r\n",
                    TAG, __FUNCTION__,
                    cPL.InverterId, cPL.InverterChannelId);

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
                MessageOutput.printf("%s%s: switch DC pre charging of inverter id %" PRIx64 " at channel %d ON\r\n",
                    TAG, __FUNCTION__,
                    cPL.InverterId, cPL.InverterChannelId);

                digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
                digitalWrite(PinMapping.get().pre_charge, LOW); // MOSfet gate on

                _lastPreCharge = millis();
                _preChargeDelay = 5 * 1000; // 5 seconds pre charge
                _preChargePowerState = 2;
            }
            return false;

        case 2: // after pre charge time (5 seconds delay) switch the full power MosFET
            if (millis() - _lastPreCharge > _preChargeDelay) {
                MessageOutput.printf("%s%s: switch DC full power of inverter %" PRIx64 " at channel %d ON\r\n",
                    TAG, __FUNCTION__,
                    cPL.InverterId, cPL.InverterChannelId);

                digitalWrite(PinMapping.get().full_power, LOW); // MOSfet gate on
                digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
                _inverter->setConnected(true);

/*
                if (!isStopThresholdReached() && !isStartThresholdReached() && cPL.Enabled != _lastDCState) {
                    if (_verboseLogging)
                        MessageOutput.printf("%s%s: initial condition (discharging allowed)\r\n", TAG, __FUNCTION__);
                    _batteryDischargeEnabled = true;
                    //                    _lastDCState = true;
                }
*/
                _lastPreCharge = millis();
                _preChargeDelay = 60 * 1000; // give 60 seconds to start Inverter
                _preChargePowerState = 3;
            }
            return false;

        case 3:
            // announceStatus(Status::WaitingInverterPowerOn);
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
    }

    return true;
}

void PowerLimiterClass::switchMosFetsOff()
{
    auto const& config = Configuration.get();

    if (_inverter)
        MessageOutput.printf("%s%s: inverter %" PRIx64 " is %s producing. AC power: %.1f\r\n", TAG, __FUNCTION__,
            config.PowerLimiter.InverterId,
            _inverter->isProducing() ? "still" : "not",
            _inverter->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));

    if (digitalRead(PinMapping.get().full_power) == LOW || digitalRead(PinMapping.get().pre_charge) == LOW) {
        MessageOutput.printf("%s%s: switch DC power of inverter id %" PRIx64 " at channel %d OFF\r\n", TAG, __FUNCTION__,
            config.PowerLimiter.InverterId,
            config.PowerLimiter.InverterChannelId);

        digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
        digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off

        _preChargePowerState = 0;
        if (_inverter) _inverter->setConnected(false);

        _lastPreCharge = millis();
        _preChargeDelay = 0;
    }

    _switchMosFetOffTimer = 0;
}
