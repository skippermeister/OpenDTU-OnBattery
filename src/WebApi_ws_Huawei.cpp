// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#ifdef USE_CHARGER_HUAWEI

#include "WebApi_ws_Huawei.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "Huawei_can.h"
#include "MessageOutput.h"
#include "Utils.h"
#include "WebApi.h"
#include "defaults.h"

WebApiWsHuaweiLiveClass::WebApiWsHuaweiLiveClass()
    : _ws("/huaweilivedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsHuaweiLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsHuaweiLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsHuaweiLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on(HttpLink, HTTP_GET, std::bind(&WebApiWsHuaweiLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsHuaweiLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
}

void WebApiWsHuaweiLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsHuaweiLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        JsonDocument root;
        JsonVariant var = root;

        generateCommonJsonResponse(var);

        if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
            String buffer;
            serializeJson(root, buffer);

            _ws.textAll(buffer);
        }
    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling %s temporarily out of resources. Reason: \"%s\".\r\n", HttpLink, bad_alloc.what());
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in %s. Reason: \"%s\".\r\n", HttpLink, exc.what());
    }
}

template <typename T>
void addInputValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["inputValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}
template <typename T>
void addOutputValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["outputValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void WebApiWsHuaweiLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    const RectifierParameters_t * rp = HuaweiCan.get();

    root["data_age"] = (millis() - HuaweiCan.getLastUpdate()) / 1000;
    addInputValue(root, "input_voltage", rp->input_voltage, "V", 2);
    addInputValue(root, "input_current", rp->input_current, "A", 1);
    addInputValue(root, "input_power", rp->input_power, "W", 0);
    addInputValue(root, "input_temp", rp->input_temp, "°C", 1);
    addInputValue(root, "efficiency", rp->efficiency * 100, "%", 1);
    addOutputValue(root, "output_voltage", rp->output_voltage, "V", 2);
    addOutputValue(root, "output_current", rp->output_current, "A", 2);
    addOutputValue(root, "max_output_current", rp->max_output_current, "A", 2);
    addOutputValue(root, "output_power", rp->output_power, "W", 0);
    addOutputValue(root, "output_temp", rp->output_temp, "°C", 1);
}

void WebApiWsHuaweiLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] connect", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] disconnect", server->url(), client->id());
    }
}

void WebApiWsHuaweiLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }
    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();

        generateCommonJsonResponse(root);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling %s temporarily out of resources. Reason: \"%s\".\r\n", HttpLink, bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in %s. Reason: \"%s\".\r\n", HttpLink, exc.what());
        WebApi.sendTooManyRequests(request);
    }
}

#endif
