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

    server.on(HttpLink, HTTP_GET, std::bind(&WebApiWsVedirectLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsVedirectLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();

    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("vedirect websocket");
    reload();
}
void WebApiWsVedirectLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) { return; }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
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
            JsonDocument root;
            JsonVariant var = root;

            generateCommonJsonResponse(var, fullUpdate);

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

    if (fullUpdate) {
        _lastFullPublish = millis();
    }
}

template <typename T>
void addValue(const JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root[name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void WebApiWsVedirectLiveClass::generateCommonJsonResponse(JsonVariant& root, bool fullUpdate)
{
    const JsonObject &array = root["vedirect"]["instances"].to<JsonObject>();
    root["vedirect"]["full_update"] = fullUpdate;

    for (size_t idx = 0; idx < VictronMppt.controllerAmount(); ++idx) {
        auto optMpptData = VictronMppt.getData(idx);
        if (!optMpptData.has_value()) { continue; }

        if (!fullUpdate && !hasUpdate(idx)) { continue; }

        String serial(optMpptData->serialNr_SER);
        if (serial.isEmpty()) { continue; } // serial required as index

        const JsonObject &nested = array[serial].to<JsonObject>();
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
    root["firmware_version"] = mpptData.getFwVersionFormatted();

    const JsonObject values = root["values"].to<JsonObject>();

    const JsonObject device = values["device"].to<JsonObject>();
    if (mpptData.hasLoad || mpptData.Capabilities.second & (1<<0)) {  // Load output present ?
        device["LOAD"] = mpptData.loadOutputState_LOAD ? "ON" : "OFF";
        if (mpptData.hasLoad || mpptData.Capabilities.second & (1<<12) ) // Load current IL in Text protocol
            addValue(device, "IL", mpptData.loadCurrent_IL_mA/1000.0, "A", 2);
        else if (mpptData.LoadCurrent.first > 0)
            addValue(device, "IL", mpptData.LoadCurrent.second / 1000.0, "A", 2);
        if (mpptData.LoadOutputVoltage.first > 0)
            addValue(device, "LoadOutputVoltage", mpptData.LoadOutputVoltage.second / 1000.0, "V", 2);
    }
    device["CS"]   = mpptData.getCsAsString();
    device["MPPT"] = mpptData.getMpptAsString();
    device["OR"]   = mpptData.getOrAsString();
    device["ERR"]  = mpptData.getErrAsString();
    addValue(device, "HSDS", mpptData.daySequenceNr_HSDS, "d", 0);
    if (mpptData.ChargerMaximumCurrent.first > 0)
        addValue(device, "ChargerMaxCurrent", mpptData.ChargerMaximumCurrent.second / 1000.0, "A", 1);
    if (mpptData.VoltageSettingsRange.first > 0) {
        device["VoltageSettingsRange"] = String(mpptData.VoltageSettingsRange.second & 0xFF)
                + " - "
                + String(mpptData.VoltageSettingsRange.second >> 8) + String(" V");
//        addValue(device, "VoltageSettingsMin", mpptData.VoltageSettingsRange.second & 0xFF, "V", 0);
//        addValue(device, "VoltageSettingsMax", mpptData.VoltageSettingsRange.second >> 8, "V", 0);
    }
    if (mpptData.MpptTemperatureMilliCelsius.first > 0)
        addValue(device, "MpptTemperature", mpptData.MpptTemperatureMilliCelsius.second / 1000.0, "°C", 1);

    // battery info
    const JsonObject output = values["output"].to<JsonObject>();
    addValue(output, "P", mpptData.batteryOutputPower_W, "W", 0);
    addValue(output, "V", mpptData.batteryVoltage_V_mV/1000.0, "V", 2);
    addValue(output, "I", mpptData.batteryCurrent_I_mA/1000.0, "A", 2);
    addValue(output, "E", mpptData.mpptEfficiency_Percent, "%", 1);
    if (mpptData.BatteryType.first > 0)
        output["BatteryType"]   = mpptData.getBatteryTypeAsString();
    if (mpptData.BatteryAbsorptionMilliVolt.first > 0)
        addValue(output, "BatteryAbsorptionVoltage", mpptData.BatteryAbsorptionMilliVolt.second / 1000.0, "V", 2);
    if (mpptData.BatteryFloatMilliVolt.first > 0)
        addValue(output, "BatteryFloatVoltage", mpptData.BatteryFloatMilliVolt.second / 1000.0, "V", 2);
    if (mpptData.BatteryMaximumCurrent.first > 0)
        addValue(output, "BatteryMaxCurrent", mpptData.BatteryMaximumCurrent.second / 1000.0, "A", 1);
    if (mpptData.SmartBatterySenseTemperatureMilliCelsius.first > 0)
        addValue(output, "BatteryTemperature", mpptData.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0, "°C", 1);

    // panel info
    const JsonObject input = values["input"].to<JsonObject>();
    if (mpptData.NetworkTotalDcInputPowerMilliWatts.first > 0)
        addValue(input, "NetworkPower", mpptData.NetworkTotalDcInputPowerMilliWatts.second / 1000.0, "W", 0);
    addValue(input, "PPV", mpptData.panelPower_PPV_W, "W", 0);
    addValue(input, "VPV", mpptData.panelVoltage_VPV_mV/1000.0, "V", 2);
    addValue(input, "IPV", mpptData.panelCurrent_mA/1000.0, "A", 2);
    addValue(input, "YieldToday", mpptData.yieldToday_H20_Wh/1000.0, "kWh", 2);
    addValue(input, "YieldYesterday", mpptData.yieldYesterday_H22_Wh/1000.0, "kWh", 2);
    addValue(input, "YieldTotal", mpptData.yieldTotal_H19_Wh/1000.0, "kWh", 2);
    addValue(input, "MaximumPowerToday", mpptData.maxPowerToday_H21_W, "W", 0);
    addValue(input, "MaximumPowerYesterday", mpptData.maxPowerYesterday_H23_W, "W", 0);
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
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();

        generateCommonJsonResponse(root, true/*fullUpdate*/);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling %s temporarily out of resources. Reason: \"%s\".\r\n", HttpLink, bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in %s. Reason: \"%s\".\r\n", HttpLink, exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
