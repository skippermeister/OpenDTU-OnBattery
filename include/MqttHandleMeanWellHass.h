// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_HASS
#ifdef USE_CHARGER_MEANWELL

#include <ArduinoJson.h>
#include <TaskSchedulerDeclarations.h>

class MqttHandleMeanWellHassClass {
public:
    MqttHandleMeanWellHassClass();
    void init(Scheduler& scheduler);
    void publishConfig();
    void forceUpdate() { _doPublish = true; };

private:
    void loop();
    void publish(const String& subtopic, const String& payload);
    void publishBinarySensor(const char* caption, const char* icon, const char* subTopic, const char* payload_on, const char* payload_off);
    void publishSensor(const char* caption, const char* icon, const char* subTopic, const char* deviceClass = NULL, const char* stateClass = NULL, const char* unitOfMeasurement = NULL);
    void createDeviceInfo(JsonObject& object);

    Task _loopTask;

    bool _wasConnected = false;
    bool _doPublish = true;
};

extern MqttHandleMeanWellHassClass MqttHandleMeanWellHass;

#endif
#endif
