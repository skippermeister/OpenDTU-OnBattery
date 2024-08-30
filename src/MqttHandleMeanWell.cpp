// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#ifdef USE_CHARGER_MEANWELL

#include "MqttHandleMeanWell.h"
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "WebApi_MeanWell.h"
#include <ctime>

static constexpr char TAG[] = "[MeanWell MQTT]";

MqttHandleMeanWellClass MqttHandleMeanWell;

MqttHandleMeanWellClass::MqttHandleMeanWellClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleMeanWellClass::loop, this))
{
}

void MqttHandleMeanWellClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    subscribeTopics();

    _lastPublish = millis();
}

void MqttHandleMeanWellClass::forceUpdate()
{
    _lastPublish = 0;
}

void MqttHandleMeanWellClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, Topic t) {
        String fullTopic(prefix + _cmdtopic.data() + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
            std::bind(&MqttHandleMeanWellClass::onMqttMessage, this, t,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6));
    };

    for (auto const& s : _subscriptions) {
        subscribe(s.first.data(), s.second);
    }
}

void MqttHandleMeanWellClass::unsubscribeTopics()
{
    String const prefix = MqttSettings.getPrefix() + _cmdtopic.data();
    for (auto const& s : _subscriptions) {
        MqttSettings.unsubscribe(prefix + s.first.data());
    }
}

#define MQTTpublish(value)                                              \
    if (!cMeanWell.UpdatesOnly || _last.value != MeanWellCan._rp.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = MeanWellCan._rp.value));

void MqttHandleMeanWellClass::loop()
{
    auto const& config = Configuration.get();

    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    if (!config.MeanWell.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) {
        callback();
    }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() || (millis() - _lastPublish) < config.Mqtt.PublishInterval * 1000) {
        return;
    }

    _lastPublish = millis();

    auto const& cMeanWell = Configuration.get().MeanWell;

    const String subtopic = "meanwell/";
    MqttSettings.publish(subtopic + "data_age", String((millis() - MeanWellCan.getLastUpdate()) / 1000));
    if (!cMeanWell.UpdatesOnly || MeanWellCan._rp.CURVE_CONFIG.CUVE != _last.CURVE_CONFIG.CUVE)
        MqttSettings.publish(subtopic + "cuve", String(_last.CURVE_CONFIG.CUVE = MeanWellCan._rp.CURVE_CONFIG.CUVE));
    if (!cMeanWell.UpdatesOnly || MeanWellCan._rp.CURVE_CONFIG.STGS != _last.CURVE_CONFIG.STGS)
        MqttSettings.publish(subtopic + "stgs", String(_last.CURVE_CONFIG.STGS = MeanWellCan._rp.CURVE_CONFIG.STGS));
    MQTTpublish(operation);
    MQTTpublish(inputVoltage);
    MQTTpublish(outputVoltage);
    MQTTpublish(outputCurrent);
    MQTTpublish(outputPower);
    MQTTpublish(outputVoltageSet);
    MQTTpublish(outputCurrentSet);
    MQTTpublish(curveCV);
    MQTTpublish(curveCC);
    MQTTpublish(curveFV);
    MQTTpublish(curveTC);
    MQTTpublish(internalTemperature);
    MQTTpublish(efficiency);
}

void MqttHandleMeanWellClass::onMqttMessage(Topic t,
    const espMqttClientTypes::MessageProperties& properties,
    const char* topic, const uint8_t* payload, size_t len,
    size_t index, size_t total)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    } catch (std::invalid_argument const& e) {
        MessageOutput.printf("%s handler: cannot parse payload of topic '%s' as float: %s\r\n", TAG, topic, strValue.c_str());
        return;
    }

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    switch (t) {
    case Topic::LimitVoltage:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Voltage: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_VOLTAGE));
        break;
    case Topic::LimitCurrent:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Current: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_CURRENT));
        break;
    case Topic::LimitCurveCV:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Curve CV: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_CURVE_CV));
        break;
    case Topic::LimitCurveCC:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Curve CC: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_CURVE_CC));
        break;
    case Topic::LimitCurveFV:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Curve FV: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_CURVE_FV));
        break;
    case Topic::LimitCurveTC:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Limit Curve TC: %.2f V\r\n", TAG, payload_val);
        _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setValue,
            &MeanWellCan, payload_val, MEANWELL_SET_CURVE_TC));
        break;
    case Topic::Mode:
        // Control power on/off
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Power Mode: %s\r\n", TAG, payload_val == 0 ? "off" : payload_val == 1 ? "on"
                    : payload_val == 2                                                                     ? "auto"
                                                                                                           : "unknown");
        switch (static_cast<int>(payload_val)) {
        case 1:
            _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setAutomaticChargeMode,
                &MeanWellCan, false));
            _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setPower,
                &MeanWellCan, true));
            break;
        case 2:
            _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setAutomaticChargeMode,
                &MeanWellCan, true));
            break;
        case 0:
            _mqttCallbacks.push_back(std::bind(&MeanWellCanClass::setAutomaticChargeMode,
                &MeanWellCan, false));
            break;
        default:
            MessageOutput.printf("%s Invalid mode %.0f\r\n", TAG, payload_val);
            break;
        }
        break;
    }
}

#endif
