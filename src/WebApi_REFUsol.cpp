// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#ifdef USE_REFUsol_INVERTER

#include "WebApi_REFUsol.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "ErrorMessages.h"
#include "REFUsolRS485Receiver.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"

void WebApiREFUsolClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/refusol/status", HTTP_GET, std::bind(&WebApiREFUsolClass::onREFUsolStatus, this, _1));
    _server->on("/api/refusol/config", HTTP_GET, std::bind(&WebApiREFUsolClass::onREFUsolAdminGet, this, _1));
    _server->on("/api/refusol/config", HTTP_POST, std::bind(&WebApiREFUsolClass::onREFUsolAdminPost, this, _1));
}

void WebApiREFUsolClass::onREFUsolStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const REFUsol_CONFIG_T& cREFUsol = Configuration.get().REFUsol;

    root["enabled"] = cREFUsol.Enabled;
    root["pollinterval"] = cREFUsol.PollInterval;
    root["updatesonly"] = cREFUsol.UpdatesOnly;
    root["verbose_logging"] = REFUsol.getVerboseLogging();

    response->setLength();
    request->send(response);
}

void WebApiREFUsolClass::onREFUsolAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onREFUsolStatus(request);
}

void WebApiREFUsolClass::onREFUsolAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& retMsg = response->getRoot();
    retMsg["type"] = Warning;

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled")
            && root.containsKey("pollinterval")
            && root.containsKey("updatesonly")
            && root.containsKey("verbose_logging"))) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["pollinterval"].as<uint32_t>() == 0) {
        retMsg["message"] = "Poll interval must be a number between 5 and 65535!";
        retMsg["code"] = WebApiError::MqttPublishInterval;
        retMsg["param"]["min"] = 5;
        retMsg["param"]["max"] = 65535;

        response->setLength();
        request->send(response);
        return;
    }

    REFUsol_CONFIG_T& cREFUsol = Configuration.get().REFUsol;
    cREFUsol.Enabled = root["enabled"].as<bool>();
    cREFUsol.UpdatesOnly = root["updatesonly"].as<bool>();
    cREFUsol.PollInterval = root["pollinterval"].as<uint32_t>();
    REFUsol.setVerboseLogging(root["verbose_logging"].as<bool>());

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    REFUsol.init();
}

#endif
