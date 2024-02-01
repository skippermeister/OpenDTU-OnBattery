// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "DalyBmsController.h"
#include "JkBmsController.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PylontechCanReceiver.h"
#include "PylontechRS485Receiver.h"
#include "VictronSmartShunt.h"

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

    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    if (!cBattery.Enabled) {
        return;
    }

    switch (cBattery.Provider) {
    case 0: // Initialize Pylontech Battery / RS485 bus
        _upProvider = std::make_unique<PylontechRS485Receiver>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#ifdef USE_PYLONTECH_CAN_RECEIVER
    case 1: // Initialize Pylontech Battery / CAN0 bus
    case 2: // Initialize Pylontech Battery / MCP2515 bus
        _upProvider = std::make_unique<PylontechCanReceiver>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#endif
#ifdef USE_JKBMS_CONTROLLER
    case 3:
        _upProvider = std::make_unique<JkBms::Controller>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#endif
#ifdef USE_VICTRON_SMART_SHUNT
    case 4:
        _upProvider = std::make_unique<VictronSmartShunt>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
    case 5:
        _upProvider = std::make_unique<DalyBMS::Controller>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#endif
    default:
        MessageOutput.printf("Unknown battery provider: %d\r\n", cBattery.Provider);
        break;
    }
}

void BatteryClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) {
        return;
    }

    _upProvider->loop();

    if (!MqttSettings.getConnected() || !_lastMqttPublish.occured()) {
        return;
    }

    _upProvider->getStats()->mqttPublish();

    _lastMqttPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
}
