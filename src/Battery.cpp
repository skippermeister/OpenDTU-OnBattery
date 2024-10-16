// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "MessageOutput.h"
#include "PylontechCanReceiver.h"
#include "PylontechRS485Receiver.h"
#include "GobelRS485Receiver.h"
#include "JkBmsController.h"
#include "JbdBmsController.h"
#include "VictronSmartShunt.h"
#include "MqttBattery.h"
#include "DalyBmsController.h"
#include "PytesCanReceiver.h"
#include "SBSCanReceiver.h"
#include "ZendureBattery.h"
#include "VictronSmartBatterySense.h"

BatteryClass Battery;

BatteryClass::BatteryClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&BatteryClass::loop, this))
{
}

std::shared_ptr<BatteryStats const> BatteryClass::getStats() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) {
        static auto sspDummyStats = std::make_shared<BatteryStats>();
        return sspDummyStats;
    }

    return _upProvider->getStats();
}

void BatteryClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    this->updateSettings();
}

void BatteryClass::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        _upProvider->deinit();
        _upProvider = nullptr;
    }

    auto const& config = Configuration.get();

    if (!config.Battery.Enabled) {
        MessageOutput.println("Battery provider not enabled");
        return;
    }

    MessageOutput.printf("Activate Battery provider using %s interface\r\n", PinMapping.get().battery.providerName);

    auto const ioProvider = PinMapping.get().battery.provider;
    switch (config.Battery.Provider) {
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_PYLONTECH_CAN_RECEIVER)
        case 0: // Initialize Pylontech Battery
            switch (ioProvider) {
#ifdef USE_PYLONTECH_RS485_RECEIVER
                case Battery_Provider_t::RS485:
                    _upProvider = std::make_unique<PylontechRS485Receiver>();
                    break;
#endif
#ifdef USE_PYLONTECH_CAN_RECEIVER
                case Battery_Provider_t::CAN0:
                case Battery_Provider_t::MCP2515:
                case Battery_Provider_t::I2C0:
                case Battery_Provider_t::I2C1:
                    _upProvider = std::make_unique<PylontechCanReceiver>();
                    break;
#endif
                default:;
            }
            break;
#endif
#ifdef USE_GOBEL_RS485_RECEIVER
        case 1:
            _upProvider = std::make_unique<GobelRS485Receiver>();
            break;
#endif
#if defined(USE_PYTES_CAN_RECEIVER) || defined(USE_PYTES_RS485_RECEIVER)
        case 2: // Initialize Pytes Battery
            switch (ioProvider) {
#ifdef USE_PYTES_RS485_RECEIVER
                case Battery_Provider_t::RS485:
                    break;
#endif
#ifdef USE_PYTES_CAN_RECEIVER
                case Battery_Provider_t::CAN0:
                case Battery_Provider_t::MCP2515:
                case Battery_Provider_t::I2C0:
                case Battery_Provider_t::I2C1:
                    _upProvider = std::make_unique<PytesCanReceiver>();
                    break;
#endif
                default:;
            }
            break;
#endif
#ifdef USE_SBS_CAN_RECEIVER
        case 3:
            switch (ioProvider) {
#ifdef USE_SBS_RS485_RECEIVER
                case Battery_Provider_t::RS485:
                    break;
#endif
#ifdef USE_SBS_CAN_RECEIVER
                case Battery_Provider_t::CAN0:
                case Battery_Provider_t::MCP2515:
                case Battery_Provider_t::I2C0:
                case Battery_Provider_t::I2C1:
                    _upProvider = std::make_unique<SBSCanReceiver>();
                    break;
#endif
                default:;
            }
            break;
#endif
#ifdef USE_JKBMS_CONTROLLER
        case 4:
            if (ioProvider == Battery_Provider_t::RS232 || ioProvider == Battery_Provider_t::RS485)
                _upProvider = std::make_unique<JkBms::Controller>();
            break;
#endif
#ifdef USE_JBDBMS_CONTROLLER
        case 5:
            _upProvider = std::make_unique<JbdBms::Controller>();
            break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
        case 6:
            if (ioProvider == Battery_Provider_t::RS232 || ioProvider == Battery_Provider_t::RS485)
                _upProvider = std::make_unique<DalyBmsController>();
            break;
#endif
#ifdef USE_VICTRON_SMART_SHUNT
        case 7:
            if (ioProvider == Battery_Provider_t::RS232)
                _upProvider = std::make_unique<VictronSmartShunt>();
            break;
#endif
#ifdef USE_VICTRON_SMART_BATTERY_SENSE
        case 8:
            _upProvider = std::make_unique<VictronSmartBatterySense>();
            break;
#endif
#ifdef USE_MQTT_BATTERY
        case 9:
            _upProvider = std::make_unique<MqttBattery>();
            break;
#endif
#ifdef USE_MQTT_ZENDURE_BATTERY
        case 10:
            _upProvider = std::make_unique<ZendureBattery>();
            break;
#endif
    }

    if (_upProvider == nullptr) {
        MessageOutput.printf("Unknown battery provider: %d at interface %s\r\n",
            config.Battery.Provider, PinMapping.get().battery.providerName);
        return;
    }

    _upProvider->_verboseLogging = config.Battery.VerboseLogging;
    if (!_upProvider->init()) { MessageOutput.println("Error battery provider"); _upProvider = nullptr; }
}

void BatteryClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) {
        return;
    }

    if (!Configuration.get().Battery.Enabled ) { return; }

    _upProvider->loop();

    _upProvider->getStats()->mqttLoop();
}

float BatteryClass::getDischargeCurrentLimit()
{
    const auto& config = Configuration.get();

    if(!config.Battery.EnableDischargeCurrentLimit) { return FLT_MAX; }

    auto dischargeCurrentLimit = config.Battery.DischargeCurrentLimit;
    auto dischargeCurrentLimitValid = dischargeCurrentLimit > 0.0f;
    auto dischargeCurrentLimitBelowSoc = config.Battery.DischargeCurrentLimitBelowSoc;
    auto dischargeCurrentLimitBelowVoltage = config.Battery.DischargeCurrentLimitBelowVoltage;
    auto statsSoCValid = getStats()->getSoCAgeSeconds() <= 60 && !config.PowerLimiter.IgnoreSoc;
    auto statsSoC = statsSoCValid ? getStats()->getSoC() : 100.0; // fail open so we use voltage instead
    auto statsVoltageValid = getStats()->getVoltageAgeSeconds() <= 60;
    auto statsVoltage = statsVoltageValid ? getStats()->getVoltage() : 0.0; // fail closed
    auto statsCurrentLimit = getStats()->getDischargeCurrentLimit();
    auto statsLimitValid = config.Battery.UseBatteryReportedDischargeLimit
        && statsCurrentLimit >= 0.0f
        && getStats()->getDischargeCurrentLimitAgeSeconds() <= 60;

    if (statsSoC > dischargeCurrentLimitBelowSoc && statsVoltage > dischargeCurrentLimitBelowVoltage) {
        // Above SoC and Voltage thresholds, ignore custom limit.
        // Battery-provided limit will still be applied.
        dischargeCurrentLimitValid = false;
    }
    if (statsLimitValid && dischargeCurrentLimitValid) {
        // take the lower limit
        return min(statsCurrentLimit, dischargeCurrentLimit);
    }

    if (statsLimitValid) {
        return statsCurrentLimit;
    }

    if (dischargeCurrentLimitValid) {
        return dischargeCurrentLimit;
    }

    return FLT_MAX;
}
