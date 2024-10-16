// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_CHARGER_MEANWELL

#include "ArduinoJson.h"
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <mutex>

class WebApiWsMeanWellLiveClass {
public:
    WebApiWsMeanWellLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void reload();

private:
    static constexpr char const HttpLink[] = "/api/meanwelllivedata/status";

    void onLivedataStatus(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebSocket _ws;
    AuthenticationMiddleware _simpleDigestAuth;

    uint32_t _lastUpdateCheck = 0;

    std::mutex _mutex;

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();
};

#endif
