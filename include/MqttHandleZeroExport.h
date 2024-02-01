// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <deque>
#include <espMqttClient.h>
#include <functional>
#include <mutex>

enum class ZeroExportTopic : unsigned {
    Enabled,
    MaxGrid,
    MinimumLimit,
    PowerHysteresis,
    Tn
};

class MqttHandleZeroExportClass {
public:
    MqttHandleZeroExportClass();
    void init(Scheduler& scheduler);

private:
    void loop();

    void onMqttMessage(ZeroExportTopic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total);

    Task _loopTask;

    TimeoutHelper _lastPublish;

    // MQTT callbacks to process updates on subscribed topics are executed in
    // the MQTT thread's context. we use this queue to switch processing the
    // user requests into the main loop's context (TaskScheduler context).
    mutable std::mutex _mqttMutex;
    std::deque<std::function<void()>> _mqttCallbacks;
};

extern MqttHandleZeroExportClass MqttHandleZeroExport;
