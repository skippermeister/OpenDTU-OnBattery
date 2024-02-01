// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler, Malte Schmidt and others
 */
#include "MqttHandlePowerLimiter.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PowerLimiter.h"
#include <ctime>

static const char TAG[] = "[PowerLimiter MQTT]";

#define TOPIC_SUB_POWER_LIMITER "mode"

MqttHandlePowerLimiterClass MqttHandlePowerLimiter;

MqttHandlePowerLimiterClass::MqttHandlePowerLimiterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandlePowerLimiterClass::loop, this))
{
}

void MqttHandlePowerLimiterClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    const String topic = MqttSettings.getPrefix() + "powerlimiter/cmd/mode";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdMode, this, _1, _2, _3, _4, _5, _6));

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
}

void MqttHandlePowerLimiterClass::loop()
{
    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    const CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter.Enabled) {
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

    const auto val = static_cast<unsigned>(PowerLimiter.getMode());
    MqttSettings.publish("powerlimiter/status/mode", String(val));

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
    yield();
}

void MqttHandlePowerLimiterClass::onCmdMode(const espMqttClientTypes::MessageProperties& properties,
    const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    int intValue = -1;
    try {
        intValue = std::stoi(strValue);
    } catch (std::invalid_argument const& e) {
        MessageOutput.printf("%s cannot parse payload of topic '%s' as int: %s\r\n", TAG, topic, strValue.c_str());
        return;
    }

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    using Mode = PowerLimiterClass::Mode;
    switch (static_cast<Mode>(intValue)) {
    case Mode::UnconditionalFullSolarPassthrough:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s unconditional full solar PT\r\n", TAG);
        _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
            &PowerLimiter, Mode::UnconditionalFullSolarPassthrough));
        break;
    case Mode::Disabled:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s disabled (override)\r\n", TAG);
        _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
            &PowerLimiter, Mode::Disabled));
        break;
    case Mode::Normal:
        if (MqttSettings.getVerboseLogging())
            MessageOutput.printf("%s Power limiter normal operation\r\n", TAG);
        _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
            &PowerLimiter, Mode::Normal));
        break;
    default:
        MessageOutput.printf("%s unknown mode %d\r\n", TAG, intValue);
        break;
    }
}
