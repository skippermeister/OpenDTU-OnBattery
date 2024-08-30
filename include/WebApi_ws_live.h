// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>

class WebApiWsLiveClass {
public:
    WebApiWsLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    static constexpr char const HttpLink[] = "/api/livedata/status";

    static void generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv);
    static void generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv);
    static void generateCommonJsonResponse(JsonVariant& root);

    void generateOnBatteryJsonResponse(JsonVariant& root, bool all);
    void sendOnBatteryStats();

    static void addField(JsonObject& root, std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic = "");
    static void addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits);

    // Daten visualisieren #168
    static void addHourPower(JsonVariant& root, std::array<float, 24> values, const String& unit, const uint8_t digits);

    void onLivedataStatus(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebSocket _ws;

    uint32_t _lastPublishOnBatteryFull = 0;
    uint32_t _lastPublishVictron = 0;
    uint32_t _lastPublishCharger = 0;
    uint32_t _lastPublishBattery = 0;
    uint32_t _lastPublishPowerMeter = 0;
    uint32_t _lastPublishHours = 0;

    uint32_t _lastPublishStats[INV_MAX_COUNT] = { 0 };

    std::mutex _mutex;

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();
};
