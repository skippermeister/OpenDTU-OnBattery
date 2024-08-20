// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_HASS
#ifdef USE_CHARGER_MEANWELL

#include "MqttHandleMeanWellHass.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "MqttHandleHass.h"
#include "MeanWell_can.h"
#include "Utils.h"
#include "__compiled_constants.h"

MqttHandleMeanWellHassClass MqttHandleMeanWellHass;

MqttHandleMeanWellHassClass::MqttHandleMeanWellHassClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleMeanWellHassClass::loop, this))
{
}
void MqttHandleMeanWellHassClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void MqttHandleMeanWellHassClass::loop()
{
    if (!Configuration.get().MeanWell.Enabled) { return; }

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

void MqttHandleMeanWellHassClass::publishConfig()
{
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled ||
        !config.MeanWell.Enabled ||
        !MqttSettings.getConnected())
    {
        return;
    }

    publishSensor("Data Age",          "mdi:timer-sand",       "data_age",           "duration",           "measurement", "s");
    publishSensor("Effeciency",        NULL,                   "efficiency",         "ChargerEfficiency",  "measurement", "%");

    publishSensor("Power",             NULL,                   "outputPower",        "ChargerPowerWatt",   "measurement", "W");
    publishSensor("Voltage",           "mdi:battery-charging", "outputVoltage",      "ChargerVoltageVolt", "measurement", "V");
    publishSensor("Current",           "mdi:current-dc",       "outputCurrent",      "ChargerCurrentAmps", "measurement", "A");
    publishSensor("Charger Temperature","mdi:thermometer",     "internalTemperature","ChargerTempCelsius", "measurement", "°C");
}

void MqttHandleMeanWellHassClass::publishSensor(const char* caption, const char* icon, const char* subTopic, const char* deviceClass, const char* stateClass, const char* unitOfMeasurement)
{
    String serial = MeanWellCan._rp.ProductSerialNo;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "meanwell/";
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
    String configTopic = "sensor/dtu_charger_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleMeanWellHassClass::publishBinarySensor(const char* caption, const char* icon, const char* subTopic, const char* payload_on, const char* payload_off)
{
    String serial = MeanWellCan._rp.ProductSerialNo;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.replace(":", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "meanwell/";
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
    String configTopic = "binary_sensor/dtu_charger_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleMeanWellHassClass::createDeviceInfo(JsonObject& object)
{
    String serial = MeanWellCan._rp.ProductSerialNo;
    object["name"] = "Charger(" + serial + ")";

    object["ids"] = serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = String(MeanWellCan._rp.ManufacturerName) + " " + String(MeanWellCan._rp.ManufacturerModelName);
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

void MqttHandleMeanWellHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic, payload, Configuration.get().Mqtt.Hass.Retain);
}

#endif
#endif
