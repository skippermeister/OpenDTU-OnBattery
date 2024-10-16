// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_REFUsol_INVERTER

#include "ArduinoJson.h"
#include <ESPAsyncWebServer.h>
#include <REFUsolRS485Receiver.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <mutex>

class WebApiWsREFUsolLiveClass {
public:
    WebApiWsREFUsolLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void reload();

private:
    static constexpr char const HttpLink[] = "/api/refusollivedata/status";

    void onLivedataStatus(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebSocket _ws;
    AuthenticationMiddleware _simpleDigestAuth;

    TimeoutHelper _lastWsPublish;
    uint32_t _newestREFUsolTimestamp = 0;

    std::mutex _mutex;
    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();
};

#endif
