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

    subscribeTopics();

    _lastPublish = millis();
}

void MqttHandleZeroExportClass::forceUpdate()
{
    _lastPublish = 0;
}

void MqttHandleZeroExportClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, Topic t) {
        String fullTopic(prefix + _cmdtopic.data() + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
            std::bind(&MqttHandleZeroExportClass::onMqttMessage, this, t,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6));
    };

    for (auto const& s : _subscriptions) {
        subscribe(s.first.data(), s.second);
    }
}

void MqttHandleZeroExportClass::unsubscribeTopics()
{
    String const prefix = MqttSettings.getPrefix() + _cmdtopic.data();
    for (auto const& s : _subscriptions) {
        MqttSettings.unsubscribe(prefix + s.first.data());
    }
}

void MqttHandleZeroExportClass::loop()
{
    auto const& config = Configuration.get();

    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    if (!config.ZeroExport.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() || (millis() - _lastPublish) < config.Mqtt.PublishInterval * 1000) {
        return;
    }

    _lastPublish = millis();

    static bool _enabled = !config.ZeroExport.Enabled;
    static int16_t _MaxGrid = -1;
    static int16_t _PowerHysteresis = -1;
    static int16_t _MinimumLimit = -1;
    static int16_t _Tn = -1;
    static int16_t _RequestedPowerLimit = -1;
    static int16_t _InverterId = -1;

    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.Enabled != _enabled)
        MqttSettings.publish("zeroexport/enabled", String(_enabled = config.ZeroExport.Enabled ? 1 : 0));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.InverterId != _InverterId)
        MqttSettings.publish("zeroexport/InverterId", String(_InverterId = config.ZeroExport.InverterId));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.MaxGrid != _MaxGrid)
        MqttSettings.publish("zeroexport/MaxGrid", String(_MaxGrid = config.ZeroExport.MaxGrid));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.PowerHysteresis != _PowerHysteresis)
        MqttSettings.publish("zeroexport/PowerHysteresis", String(_PowerHysteresis = config.ZeroExport.PowerHysteresis));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.MinimumLimit != _MinimumLimit)
        MqttSettings.publish("zeroexport/MinimumLimit", String(_MinimumLimit = config.ZeroExport.MinimumLimit));
    if (!config.ZeroExport.UpdatesOnly || config.ZeroExport.Tn != _Tn)
        MqttSettings.publish("zeroexport/Tn", String(_Tn = config.ZeroExport.Tn));
    if (!config.ZeroExport.UpdatesOnly || ZeroExport.getLastRequestedPowerLimit() != _RequestedPowerLimit)
        MqttSettings.publish("zeroexport/RequestedPowerLimit", String(_RequestedPowerLimit = ZeroExport.getLastRequestedPowerLimit()));
}

void MqttHandleZeroExportClass::onMqttMessage(Topic t,
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
    case Topic::Enabled:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s %s\r\n", TAG, payload_val == 0 ? "disabled" : "enabled");
        _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
            &ZeroExport, payload_val, Topic::Enabled));
        break;

    case Topic::MaxGrid:
        if (payload_val >= 0 && payload_val <= 2000) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s MaxGrid set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, Topic::MaxGrid));
        } else {
            MessageOutput.printf("%s MaxGrid must be between 0 and 2000\r\n", TAG);
        }
        break;

    case Topic::MinimumLimit:
        if (payload_val >= 5 && payload_val <= 20) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s MinimumLimit set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, Topic::MinimumLimit));
        } else {
            MessageOutput.printf("%s MinimumLimit must be between 5 and 20\r\n", TAG);
        }
        break;

    case Topic::PowerHysteresis:
        if (payload_val >= 1 && payload_val <= 50) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s PowerHysteresis set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, Topic::PowerHysteresis));
        } else {
            MessageOutput.printf("%s PowerHysteresis must be between 5 and 50\r\n", TAG);
        }
        break;

    case Topic::Tn:
        if (payload_val >= 1 && payload_val <= 300) {
            if (MqttSettings.getVerboseLogging())
                MessageOutput.printf("%s PowerHysteresis set to %f\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&ZeroExportClass::setParameter,
                &ZeroExport, payload_val, Topic::Tn));
        } else {
            MessageOutput.printf("%s Tn must be between 1 and 300\r\n", TAG);
        }
        break;
    }
}
