// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_HASS

#include "MqttHandleBatteryHass.h"
#include "Battery.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PylontechCanReceiver.h"
#include "Utils.h"

MqttHandleBatteryHassClass MqttHandleBatteryHass;

MqttHandleBatteryHassClass::MqttHandleBatteryHassClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleBatteryHassClass::loop, this))
{
}

void MqttHandleBatteryHassClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void MqttHandleBatteryHassClass::loop()
{
    if (!Configuration.get().Battery.Enabled) { return; }

    if (_doPublish) {
        publishConfig();
        _doPublish = false;
    }

    if (MqttSettings.getConnected() && !_wasConnected) {
        // Connection established
        _wasConnected = true;
        publishConfig();
    } else if (!MqttSettings.getConnected() && _wasConnected) {
        // Connection lost
        _wasConnected = false;
    }
}

void MqttHandleBatteryHassClass::publishConfig()
{
    CONFIG_T& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled || !config.Battery.Enabled || !MqttSettings.getConnected()) {
        return;
    }

    // device info
    // the MQTT battery provider does not re-publish the SoC under a different
    // known topic. we don't know the manufacture either. HASS auto-discovery
    // for that provider makes no sense.
    if (config.Battery.Provider != 6) {
        publishSensor("Manufacturer",          "mdi:factory",        "manufacturer");
        publishSensor("Data Age",              "mdi:timer-sand",     "dataAge",       "duration", "measurement", "s");
        publishSensor("State of Charge (SoC)", "mdi:battery-medium", "stateOfCharge", "battery",  "measurement", "%");
    }

    // battery info
    switch (config.Battery.Provider) {
#ifdef USE_PYLONTECH_CAN_RECEIVER
        case 1: // Pylontech Battery CAN0
        case 2: // Pylontech Battery CAN MCP2515
            publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
#endif
        case 0: // Pylontech Battery RS485
            publishSensor("Battery voltage", NULL, "voltage", "voltage", "measurement", "V");
            publishSensor("Battery current", NULL, "current", "current", "measurement", "A");
            publishSensor("Temperature", NULL, "temperature", "temperature", "measurement", "°C");
            publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
            publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimit", "current", "measurement", "A");
            publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimit", "current", "measurement", "A");

#define PBSA(a, b, c) publishBinarySensor("Alarm: " a, "mdi:" b, "alarm/" c, "1", "0")
#define PBSW(a, b, c) publishBinarySensor("Warning: " a, "mdi:" b, "warning/" c, "1", "0")
#define PBSC(a, b, c) publishBinarySensor(a, "mdi:" b, "charging/" c, "1", "0")
            PBSA("Discharge current",   "alert",            "overCurrentDischarge");
            PBSW("Discharge current",   "alert-outline",    "highCurrentDischarge");
            PBSA("Temperature low",     "thermometer-low",  "underTemperature");
            PBSW("Temperature low",     "thermometer-low",  "lowTemperature");
            PBSA("Temperature high",    "thermometer-high", "overTemperature");
            PBSW("Temperature high",    "thermometer-high", "highTemperature");
            PBSA("Voltage low",         "alert",            "underVoltage");
            PBSW("Voltage low",         "alert-outline",    "lowVoltage");
            PBSA("Voltage high",        "alert",            "overVoltage");
            PBSW("Voltage high",        "alert-outline",    "highVoltage");
            PBSA("BMS internal",        "alert",            "bmsInternal");
            PBSW("BMS internal",        "alert-outline",    "bmsInternal");
            PBSA("High charge current", "alert",            "overCurrentCharge");
            PBSW("High charge current", "alert-outline",    "highCurrentCharge");
            PBSC("Charge enabled",      "battery-arrow-up",   "chargeEnabled");
            PBSC("Discharge enabled",   "battery-arrow-down", "dischargeEnabled");
            PBSC("Charge immediately",  "alert",              "chargeImmediately");
#undef PBSC
#undef PBSW
#undef PBSA
            break;

#ifdef USE_JKBMS_CONTROLLER
        case 3: // JK BMS
            //            caption              icon                    topic                       dev. class     state class    unit
            publishSensor("Voltage",           "mdi:battery-charging", "BatteryVoltageMilliVolt",  "voltage",     "measurement", "mV");
            publishSensor("Current",           "mdi:current-dc",       "BatteryCurrentMilliAmps",  "current",     "measurement", "mA");
            publishSensor("BMS Temperature",   "mdi:thermometer",      "BmsTempCelsius",           "temperature", "measurement", "°C");
            publishSensor("Cell Voltage Diff", "mdi:battery-alert",    "CellDiffMilliVolt",        "voltage",     "measurement", "mV");
            publishSensor("Charge Cycles",     "mdi:counter",          "BatteryCycles");
            publishSensor("Cycle Capacity",    "mdi:battery-sync",     "BatteryCycleCapacity");

            publishBinarySensor("Charging Possible",    "mdi:battery-arrow-up",   "status/ChargingActive",    "1", "0");
            publishBinarySensor("Discharging Possible", "mdi:battery-arrow-down", "status/DischargingActive", "1", "0");
            publishBinarySensor("Balancing Active",     "mdi:scale-balance",      "status/BalancingActive",   "1", "0");
#define PBS(a, b, c) publishBinarySensor("Alarm: " a, "mdi:" b, "alarms/" c, "1", "0")
            PBS("Low Capacity",                "battery-alert-variant-outline", "LowCapacity");
            PBS("BMS Overtemperature",         "thermometer-alert",             "BmsOvertemperature");
            PBS("Charging Overvoltage",        "fuse-alert",                    "ChargingOvervoltage");
            PBS("Discharge Undervoltage",      "fuse-alert",                    "DischargeUndervoltage");
            PBS("Battery Overtemperature",     "thermometer-alert",             "BatteryOvertemperature");
            PBS("Charging Overcurrent",        "fuse-alert",                    "ChargingOvercurrent");
            PBS("Discharging Overcurrent",     "fuse-alert",                    "DischargeOvercurrent");
            PBS("Cell Voltage Difference",     "battery-alert",                 "CellVoltageDifference");
            PBS("Battery Box Overtemperature", "thermometer-alert",             "BatteryBoxOvertemperature");
            PBS("Battery Undertemperature",    "thermometer-alert",             "BatteryUndertemperature");
            PBS("Cell Overvoltage",            "battery-alert",                 "CellOvervoltage");
            PBS("Cell Undervoltage",           "battery-alert",                 "CellUndervoltage");
#undef PBS
            break;
#endif
#ifdef USE_VICTRON_SMART_SHUNT
        case 4: // Victron SmartShunt
            break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
        case 5: // DALY BMS
            break;
#endif
#ifdef USE_MQTT_BATTERY
        case 6: // SoC from MQTT
            break;
#endif
    }
}

void MqttHandleBatteryHassClass::publishSensor(const char* caption, const char* icon, const char* subTopic, const char* deviceClass, const char* stateClass, const char* unitOfMeasurement)
{
    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "battery/";
    // omit serial to avoid a breaking change
    // statTopic.concat(serial);
    // statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = serial + "_" + sensorId;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (unitOfMeasurement != NULL) {
        root["unit_of_meas"] = unitOfMeasurement;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj);

    if (Configuration.get().Mqtt.Hass.Expire) {
        root["exp_aft"] = Configuration.get().Mqtt.PublishInterval * 3;
    }
    if (deviceClass != NULL) {
        root["dev_cla"] = deviceClass;
    }
    if (stateClass != NULL) {
        root["stat_cla"] = stateClass;
    }

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    String buffer;
    serializeJson(root, buffer);
    String configTopic = "sensor/dtu_battery_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleBatteryHassClass::publishBinarySensor(const char* caption, const char* icon, const char* subTopic, const char* payload_on, const char* payload_off)
{
    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.replace(":", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "battery/";
    // omit serial to avoid a breaking change
    // statTopic.concat(serial);
    // statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["uniq_id"] = serial + "_" + sensorId;
    root["stat_t"] = statTopic;
    root["pl_on"] = payload_on;
    root["pl_off"] = payload_off;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj);

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    String buffer;
    serializeJson(root, buffer);
    String configTopic = "binary_sensor/dtu_battery_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleBatteryHassClass::createDeviceInfo(JsonObject& object)
{
    auto& config = Configuration.get();
    switch (config.Battery.Provider) {
        case 0:
        case 1:
        case 2:
        case 4:
        case 6:
            object["name"] = "Battery(" + serial + ")";
            break;
#ifdef USE_JKBMS_CONTROLLER
        case 3:
            object["name"] = "JK BMS (" + Battery.getStats()->getManufacturer() + ")";
            break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
        case 5:
            object["name"] = "DALY BMS (" + Battery.getStats()->getManufacturer() + ")";
            break;
#endif
    }

    object["ids"] = serial;
    object["cu"] = String("http://") + NetworkSettings.localIP().toString();
    object["mf"] = "OpenDTU";
    object["mdl"] = Battery.getStats()->getManufacturer();
    object["sw"] = AUTO_GIT_HASH;
}

void MqttHandleBatteryHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic, payload, Configuration.get().Mqtt.Hass.Retain);
}

#endif
