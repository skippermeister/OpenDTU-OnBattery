// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_CHARGER_MEANWELL

#include "Configuration.h"
#include <MeanWell_can.h>
#include <TaskSchedulerDeclarations.h>
#include <mutex>
#include <deque>
#include <espMqttClient.h>
#include <functional>
#include <frozen/map.h>
#include <frozen/string.h>

class MqttHandleMeanWellClass {
public:
    MqttHandleMeanWellClass();
    void init(Scheduler& scheduler);

    void forceUpdate();

    void subscribeTopics();
    void unsubscribeTopics();

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

    static constexpr frozen::string _cmdtopic = "meanwell/cmd/";
    static constexpr frozen::map<frozen::string, Topic, 7> _subscriptions = {
        { "limit_voltage", Topic::LimitVoltage },
        { "limit_current", Topic::LimitCurrent },
        { "limit_curveCV", Topic::LimitCurveCV },
        { "limit_curveCC", Topic::LimitCurveCC },
        { "limit_curveFV", Topic::LimitCurveFV },
        { "limit_curveTC", Topic::LimitCurveTC },
        { "mode",          Topic::Mode },
    };

    void loop();

    Task _loopTask;

    void onMqttMessage(Topic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total);

    RectifierParameters_t _last {};

    uint32_t _lastPublish;

    // MQTT callbacks to process updates on subscribed topics are executed in
    // the MQTT thread's context. we use this queue to switch processing the
    // user requests into the main loop's context (TaskScheduler context).
    mutable std::mutex _mqttMutex;
    std::deque<std::function<void()>> _mqttCallbacks;
};

extern MqttHandleMeanWellClass MqttHandleMeanWell;

#endif
