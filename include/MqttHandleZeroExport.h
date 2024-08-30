// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <espMqttClient.h>
#include <mutex>
#include <deque>
#include <functional>
#include <frozen/map.h>
#include <frozen/string.h>

class MqttHandleZeroExportClass {
public:
    MqttHandleZeroExportClass();
    void init(Scheduler& scheduler);

    void forceUpdate();

    void subscribeTopics();
    void unsubscribeTopics();

    enum class Topic : unsigned {
        Enabled,
        MaxGrid,
        MinimumLimit,
        PowerHysteresis,
        Tn
    };

private:
    void loop();

    static constexpr frozen::string _cmdtopic = "zeroexport/cmd/";
    static constexpr frozen::map<frozen::string, Topic, 5> _subscriptions = {
        { "enabled",         Topic::Enabled },
        { "MaxGrid",         Topic::MaxGrid },
        { "MinimumLimit",    Topic::MinimumLimit },
        { "PowerHysteresis", Topic::PowerHysteresis },
        { "Tn",              Topic::Tn },
    };

    void onMqttMessage(Topic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total);

    Task _loopTask;

    uint32_t _lastPublish;

    // MQTT callbacks to process updates on subscribed topics are executed in
    // the MQTT thread's context. we use this queue to switch processing the
    // user requests into the main loop's context (TaskScheduler context).
    mutable std::mutex _mqttMutex;
    std::deque<std::function<void()>> _mqttCallbacks;
};

extern MqttHandleZeroExportClass MqttHandleZeroExport;
