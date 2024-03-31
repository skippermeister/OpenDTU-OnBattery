// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "MessageOutput.h"
#include "DalyBmsController.h"
#include "JkBmsController.h"
#include "PylontechCanReceiver.h"
#include "PylontechRS485Receiver.h"
#include "VictronSmartShunt.h"
#include "MqttBattery.h"
#include "SerialPortManager.h"

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
    SerialPortManager.invalidateBatteryPort();

    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    if (!cBattery.Enabled) {
        MessageOutput.println("Battery provider not enabled");
        return;
    }

    switch (cBattery.Provider) {
#ifdef USE_PYLONTECH_RS485_RECEIVER
    case 0: // Initialize Pylontech Battery / RS485 bus
        _upProvider = std::make_unique<PylontechRS485Receiver>();
        if (!_upProvider->init()) {
            _upProvider = nullptr;
        }
        break;
#endif
#ifdef USE_PYLONTECH_CAN_RECEIVER
    case 1: // Initialize Pylontech Battery / CAN0 bus
    case 2: // Initialize Pylontech Battery / MCP2515 bus
        _upProvider = std::make_unique<PylontechCanReceiver>();
        break;
#endif
#ifdef USE_JKBMS_CONTROLLER
    case 3:
        _upProvider = std::make_unique<JkBms::Controller>();
        break;
#endif
#ifdef USE_VICTRON_SMART_SHUNT
    case 4:
        _upProvider = std::make_unique<VictronSmartShunt>();
        break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
    case 5:
        _upProvider = std::make_unique<DalyBmsController>();
        break;
#endif
#ifdef USE_MQTT_BATTERY
    case 6:
        _upProvider = std::make_unique<MqttBattery>();
        break;
#endif
    default:
        MessageOutput.printf("Unknown battery provider: %d\r\n", cBattery.Provider);
        break;
    }

    if(_upProvider->usesHwPort2()) {
        if (!SerialPortManager.allocateBatteryPort(2)) {
            MessageOutput.printf("[Battery] Serial port %d already in use. Initialization aborted!\r\n", 2);
            _upProvider = nullptr;
            return;
        }
    }

    if (!_upProvider->init()) {
        SerialPortManager.invalidateBatteryPort();
        _upProvider = nullptr;
    }
}

void BatteryClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) {
        return;
    }

    _upProvider->loop();

    _upProvider->getStats()->mqttLoop();
}
