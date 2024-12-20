// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#include "RestartHelper.h"
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
#include "PinMapping.h"
#include <ctime>
#include <cmath>
#include <limits>
#include <frozen/map.h>
#include "SunPosition.h"
#include "SurplusPower.h"

static constexpr char TAG[] = "[DPL::";

static auto sBatteryPoweredFilter = [](PowerLimiterInverter const& inv) {
    return !inv.isSolarPowered();
};

static const char sBatteryPoweredExpression[] = "battery-powered";

static auto sSolarPoweredFilter = [](PowerLimiterInverter const& inv) {
    return inv.isSolarPowered();
};

static const char sSolarPoweredExpression[] = "solar-powered";

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

    // to control MosFETs between battery and inverter
    // added by skippermeister
    _switchMosFetOffTimer=0;
    bool flag = false;
    bool noDCpowerSwitch = PinMapping.get().pre_charge < 0 && PinMapping.get().full_power < 0;
    if (noDCpowerSwitch) {
        MessageOutput.print("no MosFET pins configured to control DC power of inverter ");
        for (auto const& upInv : _inverters) {
//            if (!upInv->isSolarPowered())
            {
                MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
            }
        }
        MessageOutput.println();
    } else {
        // switch PowerMOSFETs for inverter DC power off (battery powered Hoymiles inverter)
        MessageOutput.print("switch DC power of inverter ");
        for (auto const& upInv : _inverters) {
//            if (!upInv->isSolarPowered())
            {
                MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
                Hoymiles.getInverterBySerial(upInv->getSerial())->setConnected(noDCpowerSwitch);
                flag = true;
            }
        }
        MessageOutput.println("OFF");
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

    if (flag) {
        MessageOutput.println("done");
    } else {
        MessageOutput.println("no battery connected Inverter for Powerlimiter configured");
    }
}

frozen::string const& PowerLimiterClass::getStatusText(PowerLimiterClass::Status status)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Status, frozen::string, 14> texts = {
        { Status::Initializing, "initializing (should not see me)" },
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::DisabledByMqtt, "disabled by MQTT" },
        { Status::WaitingForValidTimestamp, "waiting for valid date and time to be available" },
        { Status::PowerMeterPending, "waiting for sufficiently recent power meter reading" },
        { Status::InverterInvalid, "invalid inverter selection/configuration" },
        { Status::InverterCmdPending, "waiting for a start/stop/restart/limit command to complete" },
        { Status::ConfigReload, "reloading DPL configuration" },
        { Status::InverterStatsPending, "waiting for sufficiently recent inverter data" },
        { Status::UnconditionalSolarPassthrough, "unconditionally passing through all solar power (MQTT override)" },
        { Status::Stable, "the system is stable, the last power limit is still valid" },
        { Status::TemperatureRange, "temperatue out of recommended discharge range (-10°C ~ 50°C)" },
        { Status::BatteryNotInitialized, "battery is not initialized" },
        { Status::DisconnectFromBattery, "disconnect inverter from battery" }
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
    if (_lastStatus == status && (millis() -_lastStatusPrinted) < 10 * 1000) { return; }

    // after announcing once that the DPL is disabled by configuration, it
    // should just be silent while it is disabled.
    if (status == Status::DisabledByConfig && _lastStatus == status) { return; }

    MessageOutput.printf("%s%s] %s\r\n", TAG, __FUNCTION__, getStatusText(status).data());

    _lastStatus = status;
    _lastStatusPrinted = millis();
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

    for (auto& upInv : _inverters) { upInv->standby(); }
    _shutdownPending = true;

    // needed if MosFETs to control DC power of inverter between battery and inverter
    // added by skippermeister
    _switchMosFetOffTimer = millis();

    return updateInverters();
}

void PowerLimiterClass::reloadConfig()
{
    auto const& config = Configuration.get();

    _verboseLogging = config.PowerLimiter.VerboseLogging;

    // clean up all inverter instances. put inverters into
    // standby if they will not be governed any more.
    auto iter = _inverters.begin();
    while (iter != _inverters.end()) {
        bool stillGoverned = false;

        for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
            auto const& inv = config.PowerLimiter.Inverters[i];
            if (inv.Serial == 0ULL) { break; }
            stillGoverned = inv.Serial == (*iter)->getSerial() && inv.IsGoverned;
            if (stillGoverned) { break; }
        }

        if (!stillGoverned) {
            (*iter)->standby();
            if ((*iter)->update()) { return; }
        }

        iter = _inverters.erase(iter);
    }

    for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
        auto const& invConfig = config.PowerLimiter.Inverters[i];

        if (invConfig.Serial == 0ULL) { break; }

        if (!invConfig.IsGoverned) { continue; }

        auto upInv = PowerLimiterInverter::create(_verboseLogging, invConfig);
        if (upInv) { _inverters.push_back(std::move(upInv)); }
    }

    calcNextInverterRestart();

    _reloadConfigFlag = false;
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
    if (updateInverters()) { return; }

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
//        _inverter = nullptr;
    }

    if (!config.PowerLimiter.Enabled) {
        shutdown(Status::DisabledByConfig);
        return;
    }

    if (Mode::Disabled == _mode) {
        shutdown(Status::DisabledByMqtt);
        return;
    }

    if (_reloadConfigFlag) {
        reloadConfig();
        return announceStatus(Status::ConfigReload);
    }

    if (_inverters.empty()) {
        return announceStatus(Status::InverterInvalid);
    }

    // added by skippermeister
    if (usesBatteryPoweredInverter()) {
        // wait till battery provider is initialized and battery is fully scanned
        if (!Battery.initialized()) {
            shutdown(Status::BatteryNotInitialized);
            return;
        }

        // check if battery is in allowed temperature range
        if (!Battery.getStats()->isDischargeTemperatureValid()) {
            shutdown(Status::TemperatureRange);
            return;
        }

        // manage MosFets between batterie and inverter
        // added by skippermeister
        // if (manageBatteryDCpowerSwitch() == false)
        //  return;
        manageBatteryDCpowerSwitch();
    }

    uint32_t latestInverterStats = 0;
    for (auto const& upInv : _inverters) {
        auto oStatsMillis = upInv->getLatestStatsMillis();
        if (!oStatsMillis) {
            return announceStatus(Status::InverterStatsPending);
        }

        latestInverterStats = std::max(*oStatsMillis, latestInverterStats);
    }

    if (Mode::UnconditionalFullSolarPassthrough == _mode) {
        return fullSolarPassthrough(Status::UnconditionalSolarPassthrough);
    }

    // if the power meter is being used, i.e., if its data is valid, we want to
    // wait for a new reading after adjusting the inverter limit. otherwise, we
    // proceed as we will use a fallback limit independent of the power meter.
    // the power meter reading is expected to be at most 2 seconds old when it
    // arrives. this can be the case for readings provided by networked meter
    // readers, where a packet needs to travel through the network for some
    // time after the actual measurement was done by the reader.
    if (PowerMeter.isDataValid() && PowerMeter.getLastUpdate() <= (latestInverterStats + 2000)) {
        return announceStatus(Status::PowerMeterPending);
    }

    // since _lastCalculation and _calculationBackoffMs are initialized to
    // zero, this test is passed the first time the condition is checked.
    if ((millis() - _lastCalculation) < _calculationBackoffMs) {
        return announceStatus(Status::Stable);
    }

    auto autoRestartInverters = [this]() -> void {
        if (!_nextInverterRestart.first) { return; } // no automatic restarts

        auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
        auto diff = _nextInverterRestart.second - millis();
        if (diff < halfOfAllMillis) { return; }

        for (auto& upInv : _inverters) {
            if (!upInv->isSolarPowered())
            {
                MessageOutput.printf("%s%s] sending restart command to inverter %s\r\n", TAG, __FUNCTION__,
                    upInv->getSerialStr());
                upInv->restart();
            }
        }

        calcNextInverterRestart();
    };

    autoRestartInverters();

    auto getBatteryPower = [this,&config]() -> bool {
        if (!usesBatteryPoweredInverter()) { return false; }

        auto isDayPeriod = SunPosition.isDayPeriod();

        if (_nighttimeDischarging && isDayPeriod) {
            _nighttimeDischarging = false;
            return isStartThresholdReached();
        }

        if (isStopThresholdReached()) { return false; }

        if (isStartThresholdReached()) { return true; }

        if (config.PowerLimiter.BatteryAlwaysUseAtNight &&
                !isDayPeriod &&
                !_batteryDischargeEnabled) {
            _nighttimeDischarging = true;
            return true;
        }

        // we are between start and stop threshold and keep the state that was
        // last triggered, either charging or discharging.
        return _batteryDischargeEnabled;
    };

    _batteryDischargeEnabled = getBatteryPower();

    // re-calculate load-corrected voltage once (and only once) per DPL loop
    _oLoadCorrectedVoltage = std::nullopt;

    if (_verboseLogging && usesBatteryPoweredInverter()) {
        MessageOutput.printf("%s%s] battery interface %sabled, SoC %.1f %% (%s), age %u s (%s)\r\n", TAG, __FUNCTION__,
            (config.Battery.Enabled ? "en" : "dis"),
            Battery.getStats()->getSoC(),
            (config.PowerLimiter.IgnoreSoc ? "ignored" : "used"),
            Battery.getStats()->getSoCAgeSeconds(),
            (Battery.getStats()->isSoCValid()?"valid":"stale"));

        auto dcVoltage = getBatteryVoltage(true /*log voltages only once per DPL loop*/);
        MessageOutput.printf("%s%s] battery voltage %.2f V, load-corrected voltage %.2f V @ %.0f W, factor %.5f 1/A\r\n", TAG, __FUNCTION__,
            dcVoltage, getLoadCorrectedVoltage(),
            getBatteryInvertersOutputAcWatts(),
            config.PowerLimiter.VoltageLoadCorrectionFactor);

        MessageOutput.printf("%s%s] battery discharge %s, start %.2f V or %u %%, stop %.2f V or %u %%\r\n", TAG, __FUNCTION__,
                (_batteryDischargeEnabled?"allowed":"restricted"),
                config.PowerLimiter.VoltageStartThreshold,
                config.PowerLimiter.BatterySocStartThreshold,
                config.PowerLimiter.VoltageStopThreshold,
                config.PowerLimiter.BatterySocStopThreshold);

       if (config.PowerLimiter.SolarPassThroughEnabled) {
            MessageOutput.printf("%s%s] full solar-passthrough %s, start %.2f V or %u %%, stop %.2f V\r\n", TAG, __FUNCTION__,
                    (isFullSolarPassthroughActive()?"active":"dormant"),
                    config.PowerLimiter.FullSolarPassThroughStartVoltage,
                    config.PowerLimiter.FullSolarPassThroughSoc,
                    config.PowerLimiter.FullSolarPassThroughStopVoltage);
        }

        MessageOutput.printf("%s%s] start %sreached, stop %sreached, solar-passthrough %sabled, use at night: %s\r\n", TAG, __FUNCTION__,
                (isStartThresholdReached()?"":"NOT "),
                (isStopThresholdReached()?"":"NOT "),
                (config.PowerLimiter.SolarPassThroughEnabled?"en":"dis"),
                (config.PowerLimiter.BatteryAlwaysUseAtNight?"yes":"no"));

        MessageOutput.printf("%s%s] total max AC power is %u W, conduction losses are %u %%\r\n", TAG, __FUNCTION__,
            config.PowerLimiter.TotalUpperPowerLimit,
            config.PowerLimiter.ConductionLosses);
    };

    // this value is negative if we are exporting power to the grid
    // from power sources other than DPL-governed inverters.
    int16_t consumption = calcConsumption();

    uint16_t inverterTotalPower = (consumption > 0) ? static_cast<uint16_t>(consumption) : 0;

    auto totalAllowance = config.PowerLimiter.TotalUpperPowerLimit;
    inverterTotalPower = std::min(inverterTotalPower, totalAllowance);

    auto coveredBySolar = updateInverterLimits(inverterTotalPower, sSolarPoweredFilter, sSolarPoweredExpression);
    auto remaining = (inverterTotalPower >= coveredBySolar) ? inverterTotalPower - coveredBySolar : 0;
    auto powerBusUsage = calcPowerBusUsage(remaining);
    auto coveredByBattery = updateInverterLimits(powerBusUsage, sBatteryPoweredFilter, sBatteryPoweredExpression);

    if (_verboseLogging) {
        for (auto const &upInv : _inverters) { upInv->debug(); }
    }

    _lastExpectedInverterOutput = coveredBySolar + coveredByBattery;

    bool limitUpdated = updateInverters();

    _lastCalculation = millis();

    if (!limitUpdated) {
        // increase polling backoff if system seems to be stable
        _calculationBackoffMs = std::min<uint32_t>(1024, _calculationBackoffMs * 2);
        return announceStatus(Status::Stable);
    }

    _calculationBackoffMs = _calculationBackoffMsDefault;
}

std::pair<float, char const*> PowerLimiterClass::getInverterDcVoltage() {
    auto const& config = Configuration.get();

    auto iter = _inverters.begin();
    while(iter != _inverters.end()) {
        if ((*iter)->getSerial() == config.PowerLimiter.InverterSerialForDcVoltage) {
            break;
        }
        ++iter;
    }

    if (iter == _inverters.end()) {
        return { -1.0, "<unknown>" };
    }

    auto voltage = (*iter)->getDcVoltage(config.PowerLimiter.InverterChannelIdForDcVoltage);
    return { voltage, (*iter)->getSerialStr() };
}

/**
 * determines the battery's voltage, trying multiple data providers. the most
 * accurate data is expected to be delivered by a BMS, if it's available. more
 * accurate and more recent than the inverter's voltage reading is the volage
 * at the charge controller's output, if it's available. only as a fallback
 * the voltage reported by the inverter is used.
 */
float PowerLimiterClass::getBatteryVoltage(bool log) {
    auto const& config = Configuration.get();

    float res = 0;

    auto inverter = getInverterDcVoltage();
    if (inverter.first > 0) { res = inverter.first; }

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
        MessageOutput.printf("%s%s] BMS: %.2f V, MPPT: %.2f V, "
                "inverter %s: %.2f \r\n", TAG, __FUNCTION__, bmsVoltage,
                chargeControllerVoltage, inverter.second, inverter.first);
    }

    return res;
}

/**
 * calculate the AC output power (limit) to set, such that the inverter uses
 * the given power on its DC side, i.e., adjust the power for the inverter's
 * efficiency.
 */
uint16_t PowerLimiterClass::dcPowerBusToInverterAc(uint16_t dcPower)
{
    // account for losses between power bus and inverter (cables, junctions...)
    auto const& config = Configuration.get();
    float lossesFactor = 1.00 - static_cast<float>(config.PowerLimiter.ConductionLosses)/100;

    // we cannot know the efficiency at the new limit. even if we could we
    // cannot know which inverter is assigned which limit. hence we use a
    // reasonable, conservative, fixed inverter efficiency.
    return 0.95 * lossesFactor * dcPower;
}

/**
 * implements the "full solar passthrough" mode of operation. in this mode of
 * operation, the inverters shall behave as if they were connected to the solar
 * panels directly, i.e., all solar power (and only solar power) is converted
 * to AC power, independent from the power meter reading.
 */
void PowerLimiterClass::fullSolarPassthrough(PowerLimiterClass::Status reason)
{
    if ((millis() - _lastCalculation) < _calculationBackoffMs) { return; }
    _lastCalculation = millis();

    for (auto& upInv : _inverters) {
        if (upInv->isSolarPowered()) { upInv->setMaxOutput(); }
    }

    uint16_t targetOutput = 0;

    if (VictronMppt.isDataValid()) {
        targetOutput = static_cast<uint16_t>(std::max<int32_t>(0, VictronMppt.getPowerOutputWatts()));
        targetOutput = dcPowerBusToInverterAc(targetOutput);
    }

    _calculationBackoffMs = 1 * 1000;
    updateInverterLimits(targetOutput, sBatteryPoweredFilter, sBatteryPoweredExpression);
    return announceStatus(reason);
}

uint8_t PowerLimiterClass::getInverterUpdateTimeouts() const
{
    uint8_t res = 0;
    for (auto const& upInv : _inverters) {
        res += upInv->getUpdateTimeouts();
    }
    return res;
}

uint8_t PowerLimiterClass::getPowerLimiterState()
{
    bool reachable = false;
    bool producing = false;
    for (auto const& upInv : _inverters) {
        reachable |= upInv->isReachable();
        producing |= upInv->isProducing();
    }

    if (!reachable) {
        return PL_UI_STATE_INACTIVE;
    }

    if (!producing) {
        return PL_UI_STATE_CHARGING;
    }

    return _batteryDischargeEnabled ? PL_UI_STATE_USE_SOLAR_AND_BATTERY : PL_UI_STATE_USE_SOLAR_ONLY;
}

int16_t PowerLimiterClass::calcConsumption()
{
    auto const& config = Configuration.get();
    auto targetConsumption = config.PowerLimiter.TargetPowerConsumption;
    auto baseLoad = config.PowerLimiter.BaseLoadLimit;

    auto meterValid = PowerMeter.isDataValid();
    auto meterValue = PowerMeter.getPowerTotal();

    if (_verboseLogging) {
        MessageOutput.printf("%s%s] targeting %d W, base load is %u W, power meter reads %.1f W (%s)\r\n", TAG, __FUNCTION__,
                targetConsumption, baseLoad, meterValue,
                (meterValid?"valid":"stale"));
    }

    if (!meterValid) { return baseLoad; }

    auto consumption = static_cast<int16_t>(meterValue + (meterValue > 0 ? 0.5 : -0.5));

    for (auto const& upInv : _inverters) {
        if (!upInv->isBehindPowerMeter()) { continue; }

        // If the inverter is wired behind the power meter, i.e., if its
        // output is part of the power meter measurement, the produced
        // power of this inverter has to be taken into account.
        auto invOutput = upInv->getCurrentOutputAcWatts();
        consumption += invOutput;
        if (_verboseLogging) {
            MessageOutput.printf("%s%s] inverter %s is behind power meter producing %u W\r\n", TAG, __FUNCTION__,
                    upInv->getSerialStr(), invOutput);
        }
    }

    return consumption - targetConsumption;
}

/**
 * assigns new limits to all inverters matching the filter. returns the total
 * amount of power these inverters are expected to produce after the new limits
 * were applied.
 */
uint16_t PowerLimiterClass::updateInverterLimits(uint16_t powerRequested,
        PowerLimiterClass::inverter_filter_t filter, std::string const& filterExpression)
{
    std::vector<PowerLimiterInverter*> matchingInverters;
    uint16_t producing = 0; // sum of AC power the matching inverters produce now

    for (auto& upInv : _inverters) {
        if (!filter(*upInv)) { continue; }

        if (!upInv->isReachable()) {
            if (_verboseLogging) {
                MessageOutput.printf("%s%s] skipping %s as it is not reachable\r\n", TAG, __FUNCTION__,
                    upInv->getSerialStr());
            }
            continue;
        }

        if (!upInv->isSendingCommandsEnabled()) {
            if (_verboseLogging) {
                MessageOutput.printf("%s%s] skipping %s as sending commands is disabled\r\n", TAG, __FUNCTION__,
                    upInv->getSerialStr());
            }
            continue;
        }

        producing += upInv->getCurrentOutputAcWatts();
        matchingInverters.push_back(upInv.get());
    }

    int32_t diff = powerRequested - producing;

    auto const& config = Configuration.get();
    uint16_t hysteresis = config.PowerLimiter.TargetPowerConsumptionHysteresis;

    bool plural = matchingInverters.size() != 1;
    if (_verboseLogging) {
        MessageOutput.printf("%s%s] requesting %d W from %d %s inverter%s currently producing %d W (diff %i W, hysteresis %d W)\r\n",
            TAG, __FUNCTION__,
            powerRequested, matchingInverters.size(), filterExpression.c_str(),
            (plural?"s":""), producing, diff, hysteresis);
    }

    if (matchingInverters.empty()) { return 0; }

    if (std::abs(diff) < static_cast<int32_t>(hysteresis)) { return producing; }

    uint16_t covered = 0;

    if (diff < 0) {
        uint16_t reduction = static_cast<uint16_t>(diff * -1);

        uint16_t totalMaxReduction = 0;
        for (auto const pInv : matchingInverters) {
            totalMaxReduction += pInv->getMaxReductionWatts(false/*no standby*/);
        }

        // test whether we need to put at least one of the inverters into
        // standby to achieve the requested reduction.
        bool allowStandby = (totalMaxReduction < reduction);

        std::sort(matchingInverters.begin(), matchingInverters.end(),
                [allowStandby](auto const a, auto const b) {
                    auto aReduction = a->getMaxReductionWatts(allowStandby);
                    auto bReduction = b->getMaxReductionWatts(allowStandby);
                    return aReduction > bReduction;
                });

        for (auto pInv : matchingInverters) {
            auto maxReduction = pInv->getMaxReductionWatts(allowStandby);
            if (reduction >= hysteresis && maxReduction >= hysteresis) {
                reduction -= pInv->applyReduction(reduction, allowStandby);
            }
            covered += pInv->getExpectedOutputAcWatts();
        }
    }
    else {
        uint16_t increase = static_cast<uint16_t>(diff);

        std::sort(matchingInverters.begin(), matchingInverters.end(),
                [](auto const a, auto const b) {
                    return a->getMaxIncreaseWatts() > b->getMaxIncreaseWatts();
                });

        for (auto pInv : matchingInverters) {
            auto maxIncrease = pInv->getMaxIncreaseWatts();
            if (increase >= hysteresis && maxIncrease >= hysteresis) {
                increase -= pInv->applyIncrease(increase);
            }
            covered += pInv->getExpectedOutputAcWatts();
        }
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s%s] will cover %d W using %d %s inverter%s\r\n", TAG, __FUNCTION__,
            covered, matchingInverters.size(),
            filterExpression.c_str(), (plural?"s":""));
    }

    return covered;
}

// calculates how much power the battery-powered inverters shall draw from the
// power bus, which we call the part of the circuitry that is supplied by the
// solar charge controller(s), possibly an AC charger, as well as the battery.
uint16_t PowerLimiterClass::calcPowerBusUsage(uint16_t powerRequested)
{
    // We check if the PSU is on and disable battery-powered inverters in this
    // case. The PSU should reduce power or shut down first before the
    // battery-powered inverters kick in. The only case where this is not
    // desired is if the battery is over the Full Solar Passthrough Threshold.
    // In this case battery-powered inverters should produce power and the PSU
    // will shut down as a consequence.
    if (!isFullSolarPassthroughActive() && (
#ifdef USE_CHARGER_HUAWEI
        HuaweiCan.getAutoPowerStatus()
#endif
#if defined(USE_CHARGER_HUAWEI) && defined(USE_CHARGER_MEANWELL)
        ||
#endif
#ifdef USE_CHARGER_MEANWELL
        MeanWellCan.getAutoPowerStatus()
#endif
    ))
    {
        if (_verboseLogging) {
            MessageOutput.printf("%s%s] DC power bus usage blocked by AC Charger auto power\r\n", TAG, __FUNCTION__);
        }
        return 0;
    }

    auto solarOutputDc = getSolarPassthroughPower();
    auto solarOutputAc = dcPowerBusToInverterAc(solarOutputDc);
    if (isFullSolarPassthroughActive() && solarOutputAc > powerRequested) {
        if (_verboseLogging) {
            MessageOutput.printf("%s%s] using %u/%u W DC/AC from DC power bus (full solar-passthrough)\r\n", TAG, __FUNCTION__,
                solarOutputDc, solarOutputAc);
        }

        return solarOutputAc;
    }

    auto oBatteryDischargeLimit = getBatteryDischargeLimit();
    if (!oBatteryDischargeLimit) {
        if (_verboseLogging) {
            MessageOutput.printf("%s%s] granting %d W from DC power bus "
                "(no battery discharge limit), solar power is %u/%u W DC/AC\r\n",
                    TAG, __FUNCTION__,
                    powerRequested, solarOutputDc, solarOutputAc);
        }
        return powerRequested;
    }

    auto batteryAllowanceAc = dcPowerBusToInverterAc(*oBatteryDischargeLimit);

    if (_verboseLogging) {
        MessageOutput.printf("%s%s] battery allowance is %u/%u W DC/AC, solar power is %u/%u W DC/AC, requested are %u W AC\r\n",
            TAG, __FUNCTION__,
            *oBatteryDischargeLimit, batteryAllowanceAc,
            solarOutputDc, solarOutputAc, powerRequested);
    }

    uint16_t allowance = batteryAllowanceAc + solarOutputAc;
    return std::min(powerRequested, allowance);
}

bool PowerLimiterClass::updateInverters()
{
    bool busy = false;

    for (auto& upInv : _inverters) {
        if (upInv->update()) { busy = true; }
    }

    return busy;
}

uint16_t PowerLimiterClass::getSolarPassthroughPower()
{
    auto const& config = Configuration.get();

    if (!config.PowerLimiter.SolarPassThroughEnabled
            || isBelowStopThreshold()
            || !VictronMppt.isDataValid()) {
        return 0;
    }

    return VictronMppt.getPowerOutputWatts();
}

float PowerLimiterClass::getBatteryInvertersOutputAcWatts()
{
    float res = 0;

    for (auto const& upInv : _inverters) {
        if (upInv->isSolarPowered()) { continue; }
        // TODO(schlimmchen): we must use the DC power instead, as the battery
        // voltage drops proportional to the DC current draw, but the AC power
        // output does not correlate with the battery current or voltage.
        res += upInv->getCurrentOutputAcWatts();
    }

    return res;
}

std::optional<uint16_t> PowerLimiterClass::getBatteryDischargeLimit()
{
    if (!_batteryDischargeEnabled) { return 0; }

    auto currentLimit = Battery.getDischargeCurrentLimit();
    if (currentLimit == FLT_MAX) { return std::nullopt; }

    if (currentLimit <= 0) { currentLimit = -currentLimit; }

    // this uses inverter voltage since there is a voltage drop between
    // battery and inverter, so since we are regulating the inverter
    // power we should use its voltage.
    auto inverter = getInverterDcVoltage();
    if (inverter.first <= 0) {
        MessageOutput.printf("%s%s] could not determine inverter voltage\r\n", TAG, __FUNCTION__);
        return 0;
    }

    return inverter.first * currentLimit;
}

float PowerLimiterClass::getLoadCorrectedVoltage()
{
    if (_oLoadCorrectedVoltage) { return *_oLoadCorrectedVoltage; }

    auto const& config = Configuration.get();

    // TODO(schlimmchen): use the battery's data if available,
    // i.e., the current drawn from the battery as reported by the battery.
    float acPower = getBatteryInvertersOutputAcWatts();
    float dcVoltage = getBatteryVoltage();

    if (dcVoltage <= 0.0) {
        return 0.0;
    }

    _oLoadCorrectedVoltage = dcVoltage + (acPower * config.PowerLimiter.VoltageLoadCorrectionFactor);

    return *_oLoadCorrectedVoltage;
}

bool PowerLimiterClass::testThreshold(float socThreshold, float voltThreshold,
        std::function<bool(float, float)> compare)
{
    auto const& config = Configuration.get();

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
    auto const& config = Configuration.get();

    return testThreshold(
            config.PowerLimiter.BatterySocStartThreshold,
            config.PowerLimiter.VoltageStartThreshold,
            [](float a, float b) -> bool { return a >= b; }
    );
}

bool PowerLimiterClass::isStopThresholdReached()
{
    auto const& config = Configuration.get();

    return testThreshold(
            config.PowerLimiter.BatterySocStopThreshold,
            config.PowerLimiter.VoltageStopThreshold,
            [](float a, float b) -> bool { return a <= b; }
    );
}

bool PowerLimiterClass::isBelowStopThreshold()
{
    auto const& config = Configuration.get();

    return testThreshold(
            config.PowerLimiter.BatterySocStopThreshold,
            config.PowerLimiter.VoltageStopThreshold,
            [](float a, float b) -> bool { return a < b; }
    );
}

void PowerLimiterClass::calcNextInverterRestart()
{
    auto const& config = Configuration.get();

    if (config.PowerLimiter.RestartHour < 0) {
        _nextInverterRestart = { false, 0 };
        MessageOutput.printf("%s%s] automatic inverter restart disabled\r\n", TAG, __FUNCTION__);
        return;
    }

    struct tm timeinfo;
    getLocalTime(&timeinfo, 5); // always succeeds as we call this method only
                                // from the DPL loop *after* we already made
                                // sure that time information is available.

    // calculation first step is offset to next restart in minutes
    uint16_t dayMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    uint16_t targetMinutes = config.PowerLimiter.RestartHour * 60;
    uint32_t restartMillis = 0;
    if (config.PowerLimiter.RestartHour > timeinfo.tm_hour) {
        // next restart is on the same day
        restartMillis = targetMinutes - dayMinutes;
    } else {
        // next restart is on next day
        restartMillis = 1440 - dayMinutes + targetMinutes;
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s%s] Localtime read %02d:%02d / configured RestartHour %d\r\n",
            TAG, __FUNCTION__,
            timeinfo.tm_hour,
            timeinfo.tm_min, config.PowerLimiter.RestartHour);
        MessageOutput.printf("%s%s] dayMinutes %d / targetMinutes %d\r\n",
            TAG, __FUNCTION__,
            dayMinutes, targetMinutes);
        MessageOutput.printf("%s%s] next inverter restart in %d minutes\r\n",
            TAG, __FUNCTION__,
            restartMillis);
    }

    // convert unit for next restart to milliseconds and add current uptime
    restartMillis *= 60000;
    restartMillis += millis();

    MessageOutput.printf("%s%s] next inverter restart @ %d millis\r\n",
        TAG, __FUNCTION__,
        restartMillis);

    _nextInverterRestart = { true, restartMillis };
}

bool PowerLimiterClass::isFullSolarPassthroughActive()
{
    auto const& config = Configuration.get();

    // solar passthrough only applies to setups with battery-powered inverters
    if (!usesBatteryPoweredInverter()) { return false; }

    // We only do full solar PT if general solar PT is enabled
    if(!config.PowerLimiter.SolarPassThroughEnabled) { return false; }

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

bool PowerLimiterClass::usesBatteryPoweredInverter()
{
    for (auto const& upInv : _inverters) {
        if (!upInv->isSolarPowered()) { return true; }
    }

    return false;
}

bool PowerLimiterClass::isGovernedInverterProducing()
{
    for (auto const& upInv : _inverters) {
        if (upInv->isProducing()) { return true; }
    }
    return false;
}

bool PowerLimiterClass::isInverterGoverned(uint64_t serial)
{
    for (auto const& upInv : _inverters) {
        if (serial == upInv->getSerial()) {
            return true;
        }
    }

    // inverter not found and not governed via PowerLimiter
    return false;
}

bool PowerLimiterClass::isInverterSolarPowered(uint64_t serial)
{
    for (auto const& upInv : _inverters) {
        if (serial == upInv->getSerial()) {
            return upInv->isSolarPowered();
        }
    }

    // inverter not found and not governed via PowerLimiter
    // All inverters which are not governed by PowerLimiter are handled as solar powered
    return true;
}

bool PowerLimiterClass::manageBatteryDCpowerSwitch()
{
    // skip algorithm if we have not implemented the MosFETs between battery and inverter
    if (PinMapping.get().pre_charge < 0 && PinMapping.get().full_power < 0) return true;

    bool pollingEnabled = true;
    bool commandsEnabled = true;
    auto iter = _inverters.begin();
    while(iter != _inverters.end()) {
//        if ((*iter)->isSolarPowered() == false)
        {
            auto inv = Hoymiles.getInverterBySerial((*iter)->getSerial());
            if (inv->getEnablePolling() == false) pollingEnabled = false;
            if (inv->getEnableCommands() == false) commandsEnabled = false;
        }
        ++iter;
    }

    if (Configuration.get().Battery.Enabled // Battery must be enabled
        && !SunPosition.isAnnouceDayPeriod() //
        && pollingEnabled // Inverters connected to Battery must be enabled for polling
        && commandsEnabled // Inverters connected to Battery must be enabled to receive commands
        && !isStopThresholdReached() // Battery SoC must be geater than equal StopThreshold
#ifdef USE_CHARGER_HUAWEI
        && HuaweiCan.getMode() == 0
#endif
#ifdef USE_CHARGER_MEANWELL
        && MeanWellCan._rp.operation == 0
#endif
        ) // only if Charger is off
    {
        switch (_preChargePowerState) {
        case 0:
            if ((millis() - _lastPreCharge > _preChargeDelay) && (isStartThresholdReached() /*|| (!isStopThresholdReached() && _lastDCState == false)*/ )) {
                MessageOutput.printf("%s%s] switch DC power of inverter ", TAG, __FUNCTION__);
                for (auto const& upInv : _inverters) {
//                    if (!upInv->isSolarPowered())
                    {
                        MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
                    }
                }
                MessageOutput.println("OFF");
                digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off
                digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
                _batteryDischargeEnabled = false;
                Hoymiles.getInverterBySerial((*iter)->getSerial())->setConnected(false);

                _lastPreCharge = millis();
                _preChargeDelay = 30 * 1000; // 30 seconds wait before start again
                _preChargePowerState = 1;
            }
            return false;

        case 1: // switch pre charge MosFET to load capacities of Inverter (soft power on)
            if (millis() - _lastPreCharge > _preChargeDelay) {
                MessageOutput.printf("%s%s] switch DC pre charging of inverter ", TAG, __FUNCTION__);
                for (auto const& upInv : _inverters) {
//                    if (!upInv->isSolarPowered())
                    {
                        MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
                    }
                }
                MessageOutput.println(" ON");

                digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
                digitalWrite(PinMapping.get().pre_charge, LOW); // MOSfet gate on

                _lastPreCharge = millis();
                _preChargeDelay = 5 * 1000; // 5 seconds pre charge
                _preChargePowerState = 2;
            }
            return false;

        case 2: // after pre charge time (5 seconds delay) switch the full power MosFET
            if (millis() - _lastPreCharge > _preChargeDelay) {
                MessageOutput.printf("%s%s] switch DC full power of inverter ", TAG, __FUNCTION__);
                for (auto const& upInv : _inverters) {
//                    if (!upInv->isSolarPowered())
                    {
                        MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
                        Hoymiles.getInverterBySerial(upInv->getSerial())->setConnected(true);
                    }
                }
                MessageOutput.println("ON");

                digitalWrite(PinMapping.get().full_power, LOW); // MOSfet gate on
                digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off

/*
                if (!isStopThresholdReached() && !isStartThresholdReached() && cPL.Enabled != _lastDCState) {
                    if (_verboseLogging)
                        MessageOutput.printf("%s%s] initial condition (discharging allowed)\r\n", TAG, __FUNCTION__);
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
    if (!_inverters.empty()) {
        MessageOutput.printf("%s%s] inverter:\r\n", TAG, __FUNCTION__);
        for (auto const& upInv : _inverters) {
//            if (!upInv->isSolarPowered())
            {
                MessageOutput.printf("    %" PRIx64 " is %s producing. AC power: %.1f\r\n",
                    upInv->getSerial(),
                    upInv->isProducing() ? "still" : "not",
                    Hoymiles.getInverterBySerial(upInv->getSerial())->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));
            }
        }
        MessageOutput.println("OFF");
    }

    if (digitalRead(PinMapping.get().full_power) == LOW || digitalRead(PinMapping.get().pre_charge) == LOW) {
        MessageOutput.printf("%s%s] switch DC power of inverter ", TAG, __FUNCTION__);
        for (auto const& upInv : _inverters) {
//            if (!upInv->isSolarPowered())
            {
                MessageOutput.printf("%" PRIx64 " ", upInv->getSerial());
                Hoymiles.getInverterBySerial(upInv->getSerial())->setConnected(false);
            }
        }
        MessageOutput.println("OFF");

        digitalWrite(PinMapping.get().full_power, HIGH); // MOSfet gate off
        digitalWrite(PinMapping.get().pre_charge, HIGH); // MOSfet gate off

        _preChargePowerState = 0;

        _lastPreCharge = millis();
        _preChargeDelay = 0;
    }

    _switchMosFetOffTimer = 0;
}
