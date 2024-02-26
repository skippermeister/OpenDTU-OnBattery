// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_ws_vedirect_live.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PowerLimiter.h"
#include "Utils.h"
#include "VictronMppt.h"
#include "WebApi.h"
#include "defaults.h"

WebApiWsVedirectLiveClass::WebApiWsVedirectLiveClass()
    : _ws("/vedirectlivedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsVedirectLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(500 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&WebApiWsVedirectLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsVedirectLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

//    _server = &server;
    server.on("/api/vedirectlivedata/status", HTTP_GET, std::bind(&WebApiWsVedirectLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsVedirectLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    _lastWsPublish.set(10 * 1000);

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
}

void WebApiWsVedirectLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsVedirectLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    // we assume this loop to be running at least twice for every
    // update from a VE.Direct MPPT data producer, so _dataAgeMillis
    // acutally grows in between updates.
    auto lastDataAgeMillis = _dataAgeMillis;
    _dataAgeMillis = VictronMppt.getDataAgeMillis();

    // Update on ve.direct change or at least after 10 seconds
    if (!_lastWsPublish.occured() && (lastDataAgeMillis <= _dataAgeMillis)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        DynamicJsonDocument root(_responseSize);
        if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
            JsonVariant var = root;
            generateJsonResponse(var);

            String buffer;
            serializeJson(root, buffer);

            if (Configuration.get().Security.AllowReadonly) {
                _ws.setAuthentication("", "");
            } else {
                _ws.setAuthentication(AUTH_USERNAME, Configuration.get().Security.Password);
            }

            _ws.textAll(buffer);
        }

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling /api/vedirectlivedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in /api/vedirectlivedata/status. Reason: \"%s\".\r\n", exc.what());
    }

    _lastWsPublish.reset();
}

template <typename T>
void addOutputValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["output"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addInputValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["input"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void WebApiWsVedirectLiveClass::generateJsonResponse(JsonVariant& root)
{
    auto spMpptData = VictronMppt.getData();

    // device info
    root["device"]["data_age"] = VictronMppt.getDataAgeMillis() / 1000;
    root["device"]["age_critical"] = !VictronMppt.isDataValid();
    root["device"]["PID"] = spMpptData->getPidAsString();
    root["device"]["SER"] = spMpptData->SER;
    root["device"]["FW"] = spMpptData->FW;
    root["device"]["LOAD"] = spMpptData->LOAD == true ? "ON" : "OFF";
    root["device"]["CS"] = spMpptData->getCsAsString();
    root["device"]["ERR"] = spMpptData->getErrAsString();
    root["device"]["OR"] = spMpptData->getOrAsString();
    root["device"]["MPPT"] = spMpptData->getMpptAsString();
    root["device"]["HSDS"]["v"] = spMpptData->HSDS;
    root["device"]["HSDS"]["u"] = "Days";

    // battery info
    addOutputValue(root, "P", spMpptData->P, "W", 0);
    addOutputValue(root, "V", spMpptData->V, "V", 2);
    addOutputValue(root, "I", spMpptData->I, "A", 2);
    addOutputValue(root, "E", spMpptData->E, "%", 1);

    // panel info
    addInputValue(root, "PPV", spMpptData->PPV, "W", 0);
    addInputValue(root, "VPV", spMpptData->VPV, "V", 2);
    addInputValue(root, "IPV", spMpptData->IPV, "A", 2);
    addInputValue(root, "YieldToday", spMpptData->H20, "kWh", 3);
    addInputValue(root, "YieldYesterday", spMpptData->H22, "kWh", 3);
    addInputValue(root, "YieldTotal", spMpptData->H19, "kWh", 3);
    addInputValue(root, "MaximumPowerToday", spMpptData->H21, "W", 0);
    addInputValue(root, "MaximumPowerYesterday", spMpptData->H23, "W", 0);

    // power limiter state
    root["dpl"]["PLSTATE"] = Configuration.get().PowerLimiter.Enabled ? PowerLimiter.getPowerLimiterState() : -1;
    root["dpl"]["PLLIMIT"] = PowerLimiter.getLastRequestedPowerLimit();
}

void WebApiWsVedirectLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] connect\r\n", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] disconnect\r\n", server->url(), client->id());
    }
}

void WebApiWsVedirectLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }
    try {
        AsyncJsonResponse* response = new AsyncJsonResponse(false, _responseSize);
        auto& root = response->getRoot();

        generateJsonResponse(root);

        response->setLength();
        request->send(response);

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling /api/vedirectlivedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in /api/vedirectlivedata/status. Reason: \"%s\".\r\n", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
