// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler, Malte Schmidt and others
 */
#include "MqttHandleZeroExport.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "ZeroExport.h"
#include <ctime>

static constexpr char TAG[] = "[MqttHandleZeroExport]";

MqttHandleZeroExportClass MqttHandleZeroExport;

MqttHandleZeroExportClass::MqttHandleZeroExportClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleZeroExportClass::loop, this))
{
}

void MqttHandleZeroExportClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, ZeroExportTopic t) {
        String fullTopic(prefix + "zeroexport/cmd/" + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
            std::bind(&MqttHandleZeroExportClass::onMqttMessage, this, t,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6));
    };
    subscribe("enabled", ZeroExportTopic::Enabled);
    subscribe("MaxGrid", ZeroExportTopic::MaxGrid);
    subscribe("MinimumLimit", ZeroExportTopic::MinimumLimit);
    subscribe("PowerHysteresis", ZeroExportTopic::PowerHysteresis);
    subscribe("Tn", ZeroExportTopic::Tn);

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
}

void MqttHandleZeroExportClass::loop()
{
    auto const& config = Configuration.get();

    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    if (!config.ZeroExport.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) {
        callback();
    }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() || !_lastPublish.occured()) {
        return;
    }

    static bool _enabled = !config.ZeroExport.Enabled;
    static int16_t _MaxGrid = -1;
    static int16_t _PowerHysteresis = -1;
    static int16_t _MinimumLimit = -1;
    static int16_t _Tn = -1;
    static int16_t _RequestedPowerLimit = -1;
    static int16_t _InverterId = -1;

    const String topic = "zeroexport/";
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.Enabled != _enabled)
        MqttSettings.publish(topic + "enabled", String(_enabled = config.ZeroExport.Enabled ? 1 : 0));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.InverterId != _InverterId)
        MqttSettings.publish(topic + "InverterId", String(_InverterId = config.ZeroExport.InverterId));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.MaxGrid != _MaxGrid)
        MqttSettings.publish(topic + "MaxGrid", String(_MaxGrid = config.ZeroExport.MaxGrid));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.PowerHysteresis != _PowerHysteresis)
        MqttSettings.publish(topic + "PowerHysteresis", String(_PowerHysteresis = config.ZeroExport.PowerHysteresis));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.MinimumLimit != _MinimumLimit)
        MqttSettings.publish(topic + "MinimumLimit", String(_MinimumLimit = config.ZeroExport.MinimumLimit));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.Tn != _Tn)
        MqttSettings.publish(topic + "Tn", String(_Tn = config.ZeroExport.Tn));
    if (!config.ZeroExport.UpdatesOnly || ZeroExport.getLastRequestedPowerLimit() != _RequestedPowerLimit)
        MqttSettings.publish(topic + "RequestedPowerLimit", String(_RequestedPowerLimit = ZeroExport.getLastRequestedPowerLimit()));

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
    yield();
}

void MqttHandleZeroExportClass::onMqttMessage(ZeroExportTopic t,
    const espMqttClientTypes::MessageProperties& properties,
    const char* topic, const uint8_t* payload, size_t len,
    size_t index, size_t total)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    } catch (std::invalid_argument const& e) {
        MessageOutput.printf("%s handler: cannot parse payload of topic '%s' as float: %s\r\n", TAG,
            topic, strValue.c_str());
        return;
    }

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    switch (t) {
    case ZeroExportTopic::Enabled:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s %s\r\n", TAG, payload_val == 0 ? "disabled" : "enabled");
        _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
            &ZeroExport, payload_val, ZeroExportTopic::Enabled));
        break;

    case ZeroExportTopic::MaxGrid:
        if (payload_val >= 0 && payload_val <= 2000) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s MaxGrid set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, ZeroExportTopic::MaxGrid));
        } else {
            MessageOutput.printf("%s MaxGrid must be between 0 and 2000\r\n", TAG);
        }
        break;

    case ZeroExportTopic::MinimumLimit:
        if (payload_val >= 5 && payload_val <= 20) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s MinimumLimit set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, ZeroExportTopic::MinimumLimit));
        } else {
            MessageOutput.printf("%s MinimumLimit must be between 5 and 20\r\n", TAG);
        }
        break;

    case ZeroExportTopic::PowerHysteresis:
        if (payload_val >= 1 && payload_val <= 50) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s PowerHysteresis set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, ZeroExportTopic::PowerHysteresis));
        } else {
            MessageOutput.printf("%s PowerHysteresis must be between 5 and 50\r\n", TAG);
        }
        break;

    case ZeroExportTopic::Tn:
        if (payload_val >= 1 && payload_val <= 300) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s PowerHysteresis set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, ZeroExportTopic::Tn));
        } else {
            MessageOutput.printf("%s Tn must be between 1 and 300\r\n", TAG);
        }
        break;
    }
}
