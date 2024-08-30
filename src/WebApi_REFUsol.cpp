// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#ifdef USE_REFUsol_INVERTER

#include "WebApi_REFUsol.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "REFUsolRS485Receiver.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"

void WebApiREFUsolClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/refusol/status", HTTP_GET, std::bind(&WebApiREFUsolClass::onREFUsolStatus, this, _1));
    server.on("/api/refusol/config", HTTP_GET, std::bind(&WebApiREFUsolClass::onREFUsolAdminGet, this, _1));
    server.on("/api/refusol/config", HTTP_POST, std::bind(&WebApiREFUsolClass::onREFUsolAdminPost, this, _1));
}

void WebApiREFUsolClass::onREFUsolStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& cREFUsol = Configuration.get().REFUsol;

    root["enabled"] = cREFUsol.Enabled;
    root["pollinterval"] = cREFUsol.PollInterval;
    root["updatesonly"] = cREFUsol.UpdatesOnly;
    root["uss_address"] = cREFUsol.USSaddress;
    root["baudrate"] = cREFUsol.Baudrate;
    root["parity"] = cREFUsol.Parity;
    root["verbose_logging"] = cREFUsol.VerboseLogging;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
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
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root.containsKey("enabled")
            && root.containsKey("pollinterval")
            && root.containsKey("updatesonly")
            && root.containsKey("verbose_logging"))) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["pollinterval"].as<uint32_t>() == 0) {
        retMsg["message"] = "Poll interval must be a number between 1 and 60!";
        retMsg["code"] = WebApiError::MqttPublishInterval;
        retMsg["param"]["min"] = 1;
        retMsg["param"]["max"] = 60;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["uss_address"].as<uint8_t>() < 1 || root["uss_address"].as<uint8_t>() > 31) {
        retMsg["message"] = "USS address must be a number between 1 and 31!";
        retMsg["code"] = WebApiError::GenericRangeError;
        retMsg["param"]["min"] = 1;
        retMsg["param"]["max"] = 31;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& cREFUsol = Configuration.get().REFUsol;
    cREFUsol.Enabled = root["enabled"].as<bool>();
    cREFUsol.UpdatesOnly = root["updatesonly"].as<bool>();
    cREFUsol.PollInterval = root["pollinterval"].as<uint32_t>();
    cREFUsol.USSaddress = root["uss_address"].as<uint8_t>();
    cREFUsol.Baudrate = root["baudrate"].as<uint16_t>();
    cREFUsol.Parity = root["parity"].as<REFUsol_CONFIG_T::Parity_t>();
    cREFUsol.VerboseLogging = root["verbose_logging"].as<bool>();

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    REFUsol.updateSettings();
}

#endif
