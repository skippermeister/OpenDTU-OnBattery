// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#ifdef USE_HASS

#include "MqttHandleVedirectHass.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "MqttHandleHass.h"
#include "MessageOutput.h"
#include "VictronMppt.h"
#include "Utils.h"
#include "__compiled_constants.h"

MqttHandleVedirectHassClass MqttHandleVedirectHass;

MqttHandleVedirectHassClass::MqttHandleVedirectHassClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleVedirectHassClass::loop, this))
{
}

void MqttHandleVedirectHassClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void MqttHandleVedirectHassClass::loop()
{
    if (!Configuration.get().Vedirect.Enabled) {
        return;
    }
    if (_updateForced) {
        publishConfig();
        _updateForced = false;
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

void MqttHandleVedirectHassClass::forceUpdate()
{
    _updateForced = true;
}

void MqttHandleVedirectHassClass::publishConfig()
{
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled ||
       (!config.Vedirect.Enabled) ||
       (!MqttSettings.getConnected()) ||
       (!VictronMppt.isDataValid())) // and ensure data is revieved from victron
    {
        return;
    }

    // device info
    for (int idx = 0; idx < VictronMppt.controllerAmount(); ++idx) {
        auto optMpptData = VictronMppt.getData(idx);
        if (!optMpptData.has_value()) { continue; }

        publishBinarySensor("MPPT load output state", "mdi:export", "LOAD", "ON", "OFF", *optMpptData);
        publishSensor("MPPT serial number", "mdi:counter", "SER", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT firmware number", "mdi:counter", "FW", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT state of operation", "mdi:wrench", "CS", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT error code", "mdi:bell", "ERR", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT off reason", "mdi:wrench", "OR", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT tracker operation mode", "mdi:wrench", "MPPT", nullptr, nullptr, nullptr, *optMpptData);
        publishSensor("MPPT Day sequence number (0...364)", "mdi:calendar-month-outline", "HSDS", NULL, "total", "d", *optMpptData);

        // battery info
        publishSensor("Battery voltage", NULL, "V", "voltage", "measurement", "V", *optMpptData);
        publishSensor("Battery current", NULL, "I", "current", "measurement", "A", *optMpptData);
        publishSensor("Battery power (calculated)", NULL, "P", "power", "measurement", "W", *optMpptData);
        publishSensor("Battery efficiency (calculated)", NULL, "E", NULL, "measurement", "%", *optMpptData);

        // panel info
        publishSensor("Panel voltage", NULL, "VPV", "voltage", "measurement", "V", *optMpptData);
        publishSensor("Panel current (calculated)", NULL, "IPV", "current", "measurement", "A", *optMpptData);
        publishSensor("Panel power", NULL, "PPV", "power", "measurement", "W", *optMpptData);
        publishSensor("Panel yield total", NULL, "H19", "energy", "total_increasing", "kWh", *optMpptData);
        publishSensor("Panel yield today", NULL, "H20", "energy", "total", "kWh", *optMpptData);
        publishSensor("Panel maximum power today", NULL, "H21", "power", "measurement", "W", *optMpptData);
        publishSensor("Panel yield yesterday", NULL, "H22", "energy", "total", "kWh", *optMpptData);
        publishSensor("Panel maximum power yesterday", NULL, "H23", "power", "measurement", "W", *optMpptData);

        // optional info, provided only if TX is connected to charge controller
        if (optMpptData->NetworkTotalDcInputPowerMilliWatts.first != 0) {
            publishSensor("VE.Smart network total DC input power", "mdi:solar-power", "NetworkTotalDcInputPower", "power", "measurement", "W", *optMpptData);
        }
        if (optMpptData->MpptTemperatureMilliCelsius.first != 0) {
            publishSensor("MPPT temperature", "mdi:temperature-celsius", "MpptTemperature", "temperature", "measurement", "°C", *optMpptData);
        }
        if (optMpptData->SmartBatterySenseTemperatureMilliCelsius.first != 0) {
            publishSensor("Smart Battery Sense temperature", "mdi:temperature-celsius", "SmartBatterySenseTemperature", "temperature", "measurement", "°C", *optMpptData);
        }
    }

    yield();
}

void MqttHandleVedirectHassClass::publishSensor(const char *caption, const char* icon, const char *subTopic,
                                                const char *deviceClass, const char *stateClass,
                                                const char *unitOfMeasurement,
                                                const VeDirectMpptController::data_t &mpptData)
{
    String serial = mpptData.serialNr_SER;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "victron/";
    statTopic.concat(serial);
    statTopic.concat("/");
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
    createDeviceInfo(deviceObj, mpptData);

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
    String configTopic = "sensor/dtu_victron_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleVedirectHassClass::publishBinarySensor(const char *caption, const char *icon, const char *subTopic,
                                                      const char *payload_on, const char* payload_off,
                                                      const VeDirectMpptController::data_t &mpptData)
{
    String serial = mpptData.serialNr_SER;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String statTopic = MqttSettings.getPrefix() + "victron/";
    statTopic.concat(serial);
    statTopic.concat("/");
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
    createDeviceInfo(deviceObj, mpptData);

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    String buffer;
    serializeJson(root, buffer);
    String configTopic = "binary_sensor/dtu_victron_" + serial + "/" + sensorId + "/config";
    publish(configTopic, buffer);
}

void MqttHandleVedirectHassClass::createDeviceInfo(JsonObject& object,
                                                   const VeDirectMpptController::data_t &mpptData)
{
    String serial = mpptData.serialNr_SER;
    object["name"] = "Victron(" + serial + ")";
    object["ids"] = serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = mpptData.getPidAsString();
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

void MqttHandleVedirectHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic.c_str(), payload.c_str(), Configuration.get().Mqtt.Hass.Retain);
}

#endif
