// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifndef CHARGER_HUAWEI

#include "Configuration.h"
#include <MeanWell_can.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <deque>
#include <espMqttClient.h>
#include <functional>
#include <mutex>

class MqttHandleMeanWellClass {
public:
    MqttHandleMeanWellClass();
    void init(Scheduler& scheduler);

private:
    enum class Topic : unsigned {
        LimitVoltage,
        LimitCurrent,
        LimitCurveCV,
        LimitCurveCC,
        LimitCurveFV,
        LimitCurveTC,
        Mode
    };

    void loop();

    Task _loopTask;

    void onMqttMessage(Topic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total);

    RectifierParameters_t _last {};

    TimeoutHelper _lastPublish;

    // MQTT callbacks to process updates on subscribed topics are executed in
    // the MQTT thread's context. we use this queue to switch processing the
    // user requests into the main loop's context (TaskScheduler context).
    mutable std::mutex _mqttMutex;
    std::deque<std::function<void()>> _mqttCallbacks;
};

extern MqttHandleMeanWellClass MqttHandleMeanWell;

#endif
