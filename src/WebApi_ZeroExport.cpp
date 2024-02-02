// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_ZeroExport.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "ErrorMessages.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PowerMeter.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "ZeroExport.h"
#include "helper.h"

void WebApiZeroExportClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/zeroexport/status", HTTP_GET, std::bind(&WebApiZeroExportClass::onStatus, this, _1));
    _server->on("/api/zeroexport/config", HTTP_GET, std::bind(&WebApiZeroExportClass::onAdminGet, this, _1));
    _server->on("/api/zeroexport/config", HTTP_POST, std::bind(&WebApiZeroExportClass::onAdminPost, this, _1));
}

void WebApiZeroExportClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;

    root["enabled"] = cZeroExport.Enabled;
    root["updatesonly"] = cZeroExport.UpdatesOnly;
    root["verbose_logging"] = ZeroExport.getVerboseLogging();
    root["InverterId"] = cZeroExport.InverterId;
    root["MaxGrid"] = cZeroExport.MaxGrid;
    root["MinimumLimit"] = cZeroExport.MinimumLimit;
    root["PowerHysteresis"] = cZeroExport.PowerHysteresis;
    root["Tn"] = cZeroExport.Tn;

    response->setLength();
    request->send(response);
}

void WebApiZeroExportClass::onAdminGet(AsyncWebServerRequest* request)
{
    this->onStatus(request);
}

void WebApiZeroExportClass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject retMsg = response->getRoot();
    retMsg["type"] = Warning;

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    //    MessageOutput.println(json);

    if (json.length() > 2 * 1024) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(2 * 1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled")
            && root.containsKey("updatesonly")
            && root.containsKey("verbose_logging")
            && root.containsKey("InverterId")
            && root.containsKey("MaxGrid")
            && root.containsKey("PowerHysteresis")
            && root.containsKey("MinimumLimit")
            && root.containsKey("Tn"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;
    cZeroExport.Enabled = root["enabled"].as<bool>();
    cZeroExport.UpdatesOnly = root["updatesonly"].as<bool>();
    cZeroExport.MaxGrid = root["MaxGrid"].as<uint16_t>();
    cZeroExport.PowerHysteresis = root["PowerHysteresis"].as<uint16_t>();
    cZeroExport.InverterId = root["InverterId"].as<uint16_t>();
    cZeroExport.MinimumLimit = root["MinimumLimit"].as<uint16_t>();
    cZeroExport.Tn = root["Tn"].as<uint16_t>();
    ZeroExport.setVerboseLogging(root["verbose_logging"].as<bool>());

    Configuration.write();

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);
}
