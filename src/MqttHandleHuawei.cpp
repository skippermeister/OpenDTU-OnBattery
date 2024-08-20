// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#ifdef USE_CHARGER_HUAWEI

#include "MqttHandleHuawei.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "Huawei_can.h"
#include "WebApi_Huawei.h"
#include <ctime>

MqttHandleHuaweiClass MqttHandleHuawei;

static constexpr char TAG[] = "[Huawei MQTT] ";

MqttHandleHuaweiClass::MqttHandleHuaweiClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleHuaweiClass::loop, this))
{
}

void MqttHandleHuaweiClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    subscribeTopics();

    _lastPublish = millis();
}

void MqttHandleHuaweiClass::forceUpdate()
{
    _lastPublish = 0;
}

void MqttHandleHuaweiClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, Topic t) {
        String fullTopic(prefix + "huawei/cmd/" + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
                std::bind(&MqttHandleHuaweiClass::onMqttMessage, this, t,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6));
    };

    subscribe("limit_online_voltage", Topic::LimitOnlineVoltage);
    subscribe("limit_online_current", Topic::LimitOnlineCurrent);
    subscribe("limit_offline_voltage", Topic::LimitOfflineVoltage);
    subscribe("limit_offline_current", Topic::LimitOfflineCurrent);
    subscribe("mode", Topic::Mode);
}

void MqttHandleHuaweiClass::unsubscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix() + "huawei/cmd/";
    MqttSettings.unsubscribe(String(prefix + "limit_online_voltage"));
    MqttSettings.unsubscribe(String(prefix + "limit_online_current"));
    MqttSettings.unsubscribe(String(prefix + "limit_offline_voltage"));
    MqttSettings.unsubscribe(String(prefix + "limit_offline_current"));
    MqttSettings.unsubscribe(String(prefix + "mode"));
}

void MqttHandleHuaweiClass::loop()
{
    auto const& config = Configuration.get();

    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    if (!config.Huawei.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() || (millis() - _lastPublish) < config.Mqtt.PublishInterval * 1000) {
        return;
    }

    const RectifierParameters_t* rp = HuaweiCan.get();

    const String subtopic = "huawei/";
    MqttSettings.publish(subtopic + "data_age", String((millis() - HuaweiCan.getLastUpdate()) / 1000));
    MqttSettings.publish(subtopic + "input_voltage", String(rp->input_voltage));
    MqttSettings.publish(subtopic + "input_current", String(rp->input_current));
    MqttSettings.publish(subtopic + "input_power", String(rp->input_power));
    MqttSettings.publish(subtopic + "output_voltage", String(rp->output_voltage));
    MqttSettings.publish(subtopic + "output_current", String(rp->output_current));
    MqttSettings.publish(subtopic + "max_output_current", String(rp->max_output_current));
    MqttSettings.publish(subtopic + "output_power", String(rp->output_power));
    MqttSettings.publish(subtopic + "input_temp", String(rp->input_temp));
    MqttSettings.publish(subtopic + "output_temp", String(rp->output_temp));
    MqttSettings.publish(subtopic + "efficiency", String(rp->efficiency));

    _lastPublish = millis();
    yield();
}

void MqttHandleHuaweiClass::onMqttMessage(Topic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    }
    catch (std::invalid_argument const& e) {
        MessageOutput.printf("%s cannot parse payload of topic '%s' as float: %s\r\n", TAG, topic, strValue.c_str());
        return;
    }

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    switch (t) {
        case Topic::LimitOnlineVoltage:
            MessageOutput.printf("%s Limit Voltage: %f V\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setValue,
                        &HuaweiCan, payload_val, HUAWEI_ONLINE_VOLTAGE));
            break;

        case Topic::LimitOfflineVoltage:
            MessageOutput.printf("%s Offline Limit Voltage: %f V\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setValue,
                        &HuaweiCan, payload_val, HUAWEI_OFFLINE_VOLTAGE));
            break;

        case Topic::LimitOnlineCurrent:
            MessageOutput.printf("%s Limit Current: %f A\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setValue,
                        &HuaweiCan, payload_val, HUAWEI_ONLINE_CURRENT));
            break;

        case Topic::LimitOfflineCurrent:
            MessageOutput.printf("%s Offline Limit Current: %f A\r\n", TAG, payload_val);
            _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setValue,
                        &HuaweiCan, payload_val, HUAWEI_OFFLINE_CURRENT));
            break;

        case Topic::Mode:
            switch (static_cast<int>(payload_val)) {
                case 3:
                    MessageOutput.printf("%s Received MQTT msg. New mode: Full internal control\r\n", TAG);
                    _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setMode,
                                &HuaweiCan, HUAWEI_MODE_AUTO_INT));
                    break;

                case 2:
                    MessageOutput.printf("%s Received MQTT msg. New mode: Internal on/off control, external power limit\r\n", TAG);
                    _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setMode,
                                &HuaweiCan, HUAWEI_MODE_AUTO_EXT));
                    break;

                case 1:
                    MessageOutput.printf("%s Received MQTT msg. New mode: Turned ON\r\n", TAG);
                    _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setMode,
                                &HuaweiCan, HUAWEI_MODE_ON));
                    break;

                case 0:
                    MessageOutput.printf("%s Received MQTT msg. New mode: Turned OFF\r\n", TAG);
                    _mqttCallbacks.push_back(std::bind(&HuaweiCanClass::setMode,
                                &HuaweiCan, HUAWEI_MODE_OFF));
                    break;

                default:
                    MessageOutput.printf("%s Invalid mode %.0f\r\n", TAG, payload_val);
                    break;
            }
            break;
    }
}

#endif
