// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_ws_vedirect_live.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "Utils.h"
#include "WebApi.h"
#include "defaults.h"
#include "PowerLimiter.h"
#include "VictronMppt.h"

WebApiWsVedirectLiveClass::WebApiWsVedirectLiveClass()
    : _ws("/vedirectlivedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsVedirectLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1000 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&WebApiWsVedirectLiveClass::sendDataTaskCb, this))
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

    server.on("/api/vedirectlivedata/status", HTTP_GET, std::bind(&WebApiWsVedirectLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsVedirectLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

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

bool WebApiWsVedirectLiveClass::hasUpdate(size_t idx)
{
    auto dataAgeMillis = VictronMppt.getDataAgeMillis(idx);
    if (dataAgeMillis == 0) { return false; }
    auto publishAgeMillis = millis() - _lastPublish;
    return dataAgeMillis < publishAgeMillis;
}

uint16_t WebApiWsVedirectLiveClass::responseSize() const
{
    // estimated with ArduinoJson assistant
    return VictronMppt.controllerAmount() * (1024 + 1024 + 512) + 128/*DPL status and structure*/;
}

void WebApiWsVedirectLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) { return; }

    // Update on ve.direct change or at least after 10 seconds
    bool fullUpdate = (millis() - _lastFullPublish > (10 * 1000));
    bool updateAvailable = false;
    if (!fullUpdate) {
        for (size_t idx = 0; idx < VictronMppt.controllerAmount(); ++idx) {
            if (hasUpdate(idx)) {
                updateAvailable = true;
                break;
            }
        }
    }

    if (fullUpdate || updateAvailable) {
        try {
            std::lock_guard<std::mutex> lock(_mutex);
            DynamicJsonDocument root(responseSize());
            if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
                JsonVariant var = root;
                generateJsonResponse(var, fullUpdate);

                if (Utils::checkJsonOverflow(root, __FUNCTION__, __LINE__)) { return; }

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

    }

    if (fullUpdate) {
        _lastFullPublish = millis();
    }
}

template <typename T>
void addOutputValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["output"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addInputValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["input"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addDeviceValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["device"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void WebApiWsVedirectLiveClass::generateJsonResponse(JsonVariant& root, bool fullUpdate)
{
    const JsonObject &array = root["vedirect"].createNestedObject("instances");
    root["vedirect"]["full_update"] = fullUpdate;

    for (size_t idx = 0; idx < VictronMppt.controllerAmount(); ++idx) {
        auto optMpptData = VictronMppt.getData(idx);
        if (!optMpptData.has_value()) { continue; }

        if (!fullUpdate && !hasUpdate(idx)) { continue; }

        String serial(optMpptData->SER);
        if (serial.isEmpty()) { continue; } // serial required as index

        const JsonObject &nested = array.createNestedObject(serial);
        nested["data_age_ms"] = VictronMppt.getDataAgeMillis(idx);
        populateJson(nested, *optMpptData);
    }

    _lastPublish = millis();

    // power limiter state
    root["dpl"]["PLSTATE"] = (Configuration.get().PowerLimiter.Enabled) ? PowerLimiter.getPowerLimiterState() : -1;
    root["dpl"]["PLLIMIT"] = PowerLimiter.getLastRequestedPowerLimit();
}

void WebApiWsVedirectLiveClass::populateJson(const JsonObject &root, const VeDirectMpptController::data_t &mpptData) {
    // device info
    root["product_id"] = mpptData.getPidAsString();
    root["firmware_version"] = String(mpptData.FW);

    const JsonObject &values = root.createNestedObject("values");

    const JsonObject &device = values.createNestedObject("device");
    if (mpptData.Capabilities.second & (1<<0)) {  // Load output present ?
        device["LOAD"] = mpptData.LOAD ? "ON" : "OFF";
        if (mpptData.Capabilities.second & (1<<12) ) // Load current IL in Text protocol
            addDeviceValue(values, "IL", mpptData.IL, "A", 2);
        else if (mpptData.LoadCurrent.first > 0)
            addDeviceValue(values, "IL", mpptData.LoadCurrent.second / 1000.0, "A", 2);
        if (mpptData.LoadOutputVoltage.first > 0)
            addDeviceValue(values, "LoadOutputVoltage", mpptData.LoadOutputVoltage.second / 1000.0, "V", 2);
    }
    device["CS"]   = mpptData.getCsAsString();
    device["MPPT"] = mpptData.getMpptAsString();
    device["OR"]   = mpptData.getOrAsString();
    device["ERR"]  = mpptData.getErrAsString();
    addDeviceValue(root, "HSDS", mpptData.HSDS, "d", 0);
    if (mpptData.ChargerMaximumCurrent.first > 0)
        addDeviceValue(values, "ChargerMaxCurrent", mpptData.ChargerMaximumCurrent.second / 1000.0, "A", 1);
    if (mpptData.VoltageSettingsRange.first > 0) {
        device["VoltageSettingsRange"] = String(mpptData.VoltageSettingsRange.second & 0xFF)
                + " - "
                + String(mpptData.VoltageSettingsRange.second >> 8) + String(" V");
//        addDeviceValue(values, "VoltageSettingsMin", mpptData.VoltageSettingsRange.second & 0xFF, "V", 0);
//        addDeviceValue(values, "VoltageSettingsMax", mpptData.VoltageSettingsRange.second >> 8, "V", 0);
    }
    if (mpptData.MpptTemperatureMilliCelsius.first > 0)
        addDeviceValue(values, "MpptTemperature", mpptData.MpptTemperatureMilliCelsius.second / 1000.0, "°C", 1);

    // battery info
    addOutputValue(values, "P", mpptData.P, "W", 0);
    addOutputValue(values, "V", mpptData.V, "V", 2);
    addOutputValue(values, "I", mpptData.I, "A", 2);
    addOutputValue(values, "E", mpptData.E, "%", 1);
    if (mpptData.BatteryType.first > 0)
        values["output"]["BatteryType"]   = mpptData.getBatteryTypeAsString();
    if (mpptData.BatteryAbsorptionVoltage.first > 0)
        addOutputValue(values, "BatteryAbsorptionVoltage", mpptData.BatteryAbsorptionVoltage.second / 1000.0, "V", 2);
    if (mpptData.BatteryAbsorptionVoltage.first > 0)
        addOutputValue(values, "BatteryFloatVoltage", mpptData.BatteryFloatVoltage.second / 1000.0, "V", 2);
    if (mpptData.BatteryFloatVoltage.first > 0)
        addOutputValue(values, "BatteryMaxCurrent", mpptData.BatteryMaximumCurrent.second / 1000.0, "A", 1);
    if (mpptData.SmartBatterySenseTemperatureMilliCelsius.first > 0)
        addOutputValue(values, "BatteryTemperature", mpptData.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0, "°C", 1);

    // panel info
    if (mpptData.NetworkTotalDcInputPowerMilliWatts.first > 0)
        addInputValue(values, "NetworkPower", mpptData.NetworkTotalDcInputPowerMilliWatts.second / 1000.0, "W", 0);
    addInputValue(values, "PPV", mpptData.PPV, "W", 0);
    addInputValue(values, "VPV", mpptData.VPV, "V", 2);
    addInputValue(values, "IPV", mpptData.IPV, "A", 2);
    addInputValue(values, "YieldToday", mpptData.H20, "kWh", 3);
    addInputValue(values, "YieldYesterday", mpptData.H22, "kWh", 3);
    addInputValue(values, "YieldTotal", mpptData.H19, "kWh", 3);
    addInputValue(values, "MaximumPowerToday", mpptData.H21, "W", 0);
    addInputValue(values, "MaximumPowerYesterday", mpptData.H23, "W", 0);
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
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse(false, responseSize());
        auto& root = response->getRoot();

        generateJsonResponse(root, true/*fullUpdate*/);

        if (Utils::checkJsonOverflow(root, __FUNCTION__, __LINE__)) { return; }

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
