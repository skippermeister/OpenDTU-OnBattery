// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#ifdef USE_REFUsol_INVERTER

#include "WebApi_ws_REFUsol_live.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "Utils.h"
#include "WebApi.h"
#include "defaults.h"

WebApiWsREFUsolLiveClass::WebApiWsREFUsolLiveClass()
    : _ws("/refusollivedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsREFUsolLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsREFUsolLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsREFUsolLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _server = &server;
    _server->on("/api/refusollivedata/status", HTTP_GET, std::bind(&WebApiWsREFUsolLiveClass::onLivedataStatus, this, _1));

    _server->addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsREFUsolLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    _lastWsPublish.set(10 * 1000);

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
}

void WebApiWsREFUsolLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();

    if (Configuration.get().Security.AllowReadonly) {
        _ws.setAuthentication("", "");
    } else {
        _ws.setAuthentication(AUTH_USERNAME, Configuration.get().Security.Password);
    }
}

void WebApiWsREFUsolLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    // Update on REFUsol change or at least after 10 seconds
    if (!_lastWsPublish.occured() && (REFUsol.getLastUpdate() <= _newestREFUsolTimestamp + 5 * 1000)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        DynamicJsonDocument root(3 * 1024);
        if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
            JsonVariant var = root;
            REFUsol.generateJsonResponse(var);

            String buffer;
            serializeJson(root, buffer);
            _newestREFUsolTimestamp = std::max<uint32_t>(_newestREFUsolTimestamp, REFUsol.getLastUpdate());

            _ws.textAll(buffer);
        }

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling /api/refusollivedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in /api/refusollivedata/status. Reason: \"%s\".\r\n", exc.what());
    }

    _lastWsPublish.reset();
}

void WebApiWsREFUsolLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] connect\r\n", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] disconnect\r\n", server->url(), client->id());
    }
}

void WebApiWsREFUsolLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }
    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse(false, 3 * 1024U);
        auto& root = response->getRoot();

        REFUsol.generateJsonResponse(root);
        if (REFUsol.getLastUpdate() > _newestREFUsolTimestamp) {
            _newestREFUsolTimestamp = REFUsol.getLastUpdate();
        }

        response->setLength();
        request->send(response);

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling /api/refusollivedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in /api/refusollivedata/status. Reason: \"%s\".\r\n", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
#endif
