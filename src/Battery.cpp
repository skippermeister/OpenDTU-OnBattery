// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "MessageOutput.h"
#include "PylontechCanReceiver.h"
#include "PylontechRS485Receiver.h"
#include "JkBmsController.h"
#include "VictronSmartShunt.h"
#include "MqttBattery.h"
#include "DalyBmsController.h"
#include "PytesCanReceiver.h"

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
        MessageOutput.println("Battery provider not enabled");
        return;
    }

    MessageOutput.println("Activate Battery provider");

    switch (cBattery.Provider) {
#ifdef USE_PYLONTECH_RS485_RECEIVER
        case 0: // Initialize Pylontech Battery / RS485 bus
            _upProvider = std::make_unique<PylontechRS485Receiver>();
            break;
#endif
#ifdef USE_PYLONTECH_CAN_RECEIVER
        case 1: // Initialize Pylontech Battery / CAN0 bus
        case 2: // Initialize Pylontech Battery / MCP2515 bus
            _upProvider = std::make_unique<PylontechCanReceiver>(cBattery.Provider==1);
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
#ifdef USE_PYTES_CAN_RECEIVER
        case 7: // Initialize Pylontech Battery / CAN0 bus
        case 8: // Initialize Pylontech Battery / MCP2515 bus
            _upProvider = std::make_unique<PytesCanReceiver>(cBattery.Provider==7);
            break;
#endif
        default:
            MessageOutput.printf("Unknown battery provider: %d\r\n", cBattery.Provider);
            return;
    }

    _upProvider->_verboseLogging = Configuration.get().Battery.VerboseLogging;
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
