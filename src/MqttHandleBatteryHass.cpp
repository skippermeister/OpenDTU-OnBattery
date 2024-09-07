// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_HASS

#include "MqttHandleBatteryHass.h"
#include "Battery.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "MqttHandleHass.h"
#include "PylontechCanReceiver.h"
#include "Utils.h"
#include "__compiled_constants.h"

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
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled ||
        !config.Battery.Enabled ||
        !MqttSettings.getConnected())
    {
        return;
    }

    // device info
    // the MQTT battery provider does not re-publish the SoC under a different
    // known topic. we don't know the manufacture either. HASS auto-discovery
    // for that provider makes no sense.
    if (config.Battery.Provider != 8) {
        publishSensor("Manufacturer",          "mdi:factory",        "manufacturer");
        publishSensor("Data Age",              "mdi:timer-sand",     "dataAge",       "duration", "measurement", "s");
        publishSensor("State of Charge (SoC)", "mdi:battery-medium", "stateOfCharge", "battery",  "measurement", "%");
    }

    // battery info
    switch (config.Battery.Provider) {
        case 0: // Pylontech Battery via CAN0, MCP2515, I2C0, I2C1, RS485
#ifdef USE_PYLONTECH_CAN_RECEIVER
            if (PinMapping.get().battery.provider != Battery_Provider_t::RS485)
                publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
#endif
            publishSensor("Battery voltage", NULL, "voltage", "voltage", "measurement", "V");
            publishSensor("Battery current", NULL, "current", "current", "measurement", "A");
            publishSensor("Total Capacity", NULL, "capacity", "capacity", "measurement", "Ah");
            publishSensor("Remaining Capacity", NULL, "remainingCapacity", "capacity", "measurement", "Ah");
            publishSensor("Charge Cycles",     "mdi:counter",     "cycles");
            publishSensor("Cell voltage (diff)", NULL, "cellVoltages/cellDiffVoltage", "voltage", "measurement", "V");
            publishSensor("Cell voltage (max)",  NULL, "cellVoltages/cellMaxVoltage", "voltage", "measurement", "V");
            publishSensor("Cell voltage (min)",  NULL, "cellVoltages/cellMinVoltage", "voltage", "measurement", "V");
            publishSensor("Temperature (BMS)",      NULL, "temperatures/averageBMSTemperature", "temperature", "measurement", "°C");
            publishSensor("Cell Temperature (max)", NULL, "temperatures/maxCellTemperature", "temperature", "measurement", "°C");
            publishSensor("Cell Temperature (min)", NULL, "temperatures/minCellTemperature", "temperature", "measurement", "°C");
            publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
            publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimit", "current", "measurement", "A");
            publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimit", "current", "measurement", "A");
            publishSensor("Module Count", "midi:counter", "modulesTotal");

#define PBSA(a, b, c) publishBinarySensor("Alarm: " a, "mdi:" b, "alarms/" c, "1", "0")
#define PBSW(a, b, c) publishBinarySensor("Warning: " a, "mdi:" b, "warnings/" c, "1", "0")
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
            PBSC("Full charge request", "alert",              "fullChargeRequest");
#undef PBSC
#undef PBSW
#undef PBSA
            break;

#ifdef USE_GOBAL_RS485_RECEIVER
        case 1: // GOBEL GP-RN150 via RS485
            break;
#endif

#ifdef USE_PYTES_CAN_RECEIVER
        case 2: // Pytes Battery via CAN0, MCP2515, I2C0, I2C1
            publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
            publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimitation", "current", "measurement", "A");
            publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
            publishSensor("Discharge voltage limit", NULL, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V");

            publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
            publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");
            publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
            publishSensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");

            publishSensor("Charged Energy", NULL, "chargedEnergy", "energy", "total_increasing", "kWh");
            publishSensor("Discharged Energy", NULL, "dischargedEnergy", "energy", "total_increasing", "kWh");

            publishSensor("Total Capacity", NULL, "capacity");
            publishSensor("Available Capacity", NULL, "availableCapacity");

            publishSensor("Cell Min Voltage", NULL, "CellMinMilliVolt", "voltage", "measurement", "mV");
            publishSensor("Cell Max Voltage", NULL, "CellMaxMilliVolt", "voltage", "measurement", "mV");
            publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
            publishSensor("Cell Min Temperature", NULL, "CellMinTemperature", "temperature", "measurement", "°C");
            publishSensor("Cell Max Temperature", NULL, "CellMaxTemperature", "temperature", "measurement", "°C");

            publishSensor("Cell Min Voltage Label", NULL, "CellMinVoltageName");
            publishSensor("Cell Max Voltage Label", NULL, "CellMaxVoltageName");
            publishSensor("Cell Min Temperature Label", NULL, "CellMinTemperatureName");
            publishSensor("Cell Max Temperature Label", NULL, "CellMaxTemperatureName");

            publishSensor("Modules Online", "mdi:counter", "modulesOnline");
            publishSensor("Modules Offline", "mdi:counter", "modulesOffline");
            publishSensor("Modules Blocking Charge", "mdi:counter", "modulesBlockingCharge");
            publishSensor("Modules Blocking Discharge", "mdi:counter", "modulesBlockingDischarge");

            publishBinarySensor("Alarm Discharge current", "mdi:alert", "alarm/overCurrentDischarge", "1", "0");
            publishBinarySensor("Alarm High charge current", "mdi:alert", "alarm/overCurrentCharge", "1", "0");
            publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
            publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
            publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
            publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
            publishBinarySensor("Alarm Temperature low (charge)", "mdi:thermometer-low", "alarm/underTemperatureCharge", "1", "0");
            publishBinarySensor("Alarm Temperature high (charge)", "mdi:thermometer-high", "alarm/overTemperatureCharge", "1", "0");
            publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");
            publishBinarySensor("Alarm Cell Imbalance", "mdi:alert-outline", "alarm/cellImbalance", "1", "0");

            publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");
            publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");
            publishBinarySensor("Warning Voltage low", "mdi:alert-outline", "warning/lowVoltage", "1", "0");
            publishBinarySensor("Warning Voltage high", "mdi:alert-outline", "warning/highVoltage", "1", "0");
            publishBinarySensor("Warning Temperature low", "mdi:thermometer-low", "warning/lowTemperature", "1", "0");
            publishBinarySensor("Warning Temperature high", "mdi:thermometer-high", "warning/highTemperature", "1", "0");
            publishBinarySensor("Warning Temperature low (charge)", "mdi:thermometer-low", "warning/lowTemperatureCharge", "1", "0");
            publishBinarySensor("Warning Temperature high (charge)", "mdi:thermometer-high", "warning/highTemperatureCharge", "1", "0");
            publishBinarySensor("Warning BMS internal", "mdi:alert-outline", "warning/bmsInternal", "1", "0");
            publishBinarySensor("Warning Cell Imbalance", "mdi:alert-outline", "warning/cellImbalance", "1", "0");
            break;
#endif
#ifdef USE_SBS_CAN_RECEIVER
        case 3: // SBS CAN RECEIVER
            publishSensor("Battery voltage", NULL, "voltage", "voltage", "measurement", "V");
            publishSensor("Battery current", NULL, "current", "current", "measurement", "A");
            publishSensor("Temperature", NULL, "temperature", "temperature", "measurement", "°C");
            publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
            publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
            publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimitation", "current", "measurement", "A");
            publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimitation", "current", "measurement", "A");

            publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
            publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
            publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
            publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
            publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");

            publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");
            publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");


            publishBinarySensor("Charge enabled", "mdi:battery-arrow-up", "charging/chargeEnabled", "1", "0");
            publishBinarySensor("Discharge enabled", "mdi:battery-arrow-down", "charging/dischargeEnabled", "1", "0");

            break;
#endif
#ifdef USE_JKBMS_CONTROLLER
        case 4: // JK BMS
            //            caption              icon                    topic                       dev. class     state class    unit
            publishSensor("Voltage",           "mdi:battery-charging", "BatteryVoltageMilliVolt",  "voltage",     "measurement", "mV");
            publishSensor("Current",           "mdi:current-dc",       "BatteryCurrentMilliAmps",  "current",     "measurement", "mA");
            publishSensor("BMS Temperature",   "mdi:thermometer",      "BmsTempCelsius",           "temperature", "measurement", "°C");
            publishSensor("Cell Voltage Diff", "mdi:battery-alert",    "CellDiffMilliVolt",        "voltage",     "measurement", "mV");
            publishSensor("Battery Temperature 1", "mdi:thermometer",  "BatteryTempOneCelsius",    "temperature", "measurement", "°C");
            publishSensor("Battery Temperature 2", "mdi:thermometer",  "BatteryTempTwoCelsius",    "temperature", "measurement", "°C");
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
#ifdef USE_JBDBMS_CONTROLLER
        case 5: // JBD BMS Controller
            break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
        case 6: // DALY BMS
            break;
#endif
#ifdef USE_VICTRON_SMART_SHUNT
        case 7: // Victron SmartShunt
            publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
            publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");
            publishSensor("Instantaneous Power", NULL, "instantaneousPower", "power", "measurement", "W");
            publishSensor("Charged Energy", NULL, "chargedEnergy", "energy", "total_increasing", "kWh");
            publishSensor("Discharged Energy", NULL, "dischargedEnergy", "energy", "total_increasing", "kWh");
            publishSensor("Charge Cycles", "mdi:counter", "chargeCycles");
            publishSensor("Consumed Amp Hours", NULL, "consumedAmpHours", NULL, "measurement", "Ah");
            publishSensor("Last Full Charge", "mdi:timelapse", "lastFullCharge", NULL, NULL, "min");
            publishSensor("Midpoint Voltage", NULL, "midpointVoltage", "voltage", "measurement", "V");
            publishSensor("Midpoint Deviation", NULL, "midpointDeviation", "battery", "measurement", "%");
            break;
#endif
#ifdef USE_MQTT_BATTERY
        case 8: // SoC from MQTT
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
        case 5:
        case 6:
            object["name"] = "Battery(" + serial + ")";
            break;
#ifdef USE_JKBMS_CONTROLLER
        case 3:
            object["name"] = "JK BMS (" + Battery.getStats()->getManufacturer() + ")";
            break;
#endif
#ifdef USE_DALYBMS_CONTROLLER
        case 4:
            object["name"] = "DALY BMS (" + Battery.getStats()->getManufacturer() + ")";
            break;
#endif
    }

    object["ids"] = serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = Battery.getStats()->getManufacturer();
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

void MqttHandleBatteryHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic, payload, Configuration.get().Mqtt.Hass.Retain);
}

#endif
