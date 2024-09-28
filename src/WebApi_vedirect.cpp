// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_vedirect.h"
#include "VictronMppt.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include "MqttHandlePowerLimiterHass.h"

void WebApiVedirectClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/vedirect/status", HTTP_GET, std::bind(&WebApiVedirectClass::onVedirectStatus, this, _1));
    server.on("/api/vedirect/config", HTTP_GET, std::bind(&WebApiVedirectClass::onVedirectAdminGet, this, _1));
    server.on("/api/vedirect/config", HTTP_POST, std::bind(&WebApiVedirectClass::onVedirectAdminPost, this, _1));
}

void WebApiVedirectClass::onVedirectStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& config = Configuration.get();

    root["enabled"] = config.Vedirect.Enabled;
    root["updatesonly"] = config.Vedirect.UpdatesOnly;
    root["verbose_logging"] = VictronMppt.getVerboseLogging();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiVedirectClass::onVedirectAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onVedirectStatus(request);
}

void WebApiVedirectClass::onVedirectAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!root["enabled"].is<bool>() ||
        !root["updatesonly"].is<bool>() ||
        !root["verbose_logging"].is<bool>())
    {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.Vedirect.Enabled = root["enabled"];
    config.Vedirect.UpdatesOnly = root["updatesonly"];
    VictronMppt.setVerboseLogging(root["verbose_logging"]);

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    VictronMppt.updateSettings();

    // potentially make solar passthrough thresholds auto-discoverable
    #ifdef USE_HASS
    MqttHandlePowerLimiterHass.forceUpdate();
    #endif
}
