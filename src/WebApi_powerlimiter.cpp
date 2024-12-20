// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_powerlimiter.h"
#include "VeDirectFrameHandler.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MqttHandlePowerLimiterHass.h"
#include "MqttHandleVedirectHass.h"
#include "PowerLimiter.h"
#include "WebApi.h"
#include "helper.h"
#include "WebApi_errors.h"

void WebApiPowerLimiterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/powerlimiter/status", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onStatus, this, _1));
    server.on("/api/powerlimiter/config", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onAdminGet, this, _1));
    server.on("/api/powerlimiter/config", HTTP_POST, std::bind(&WebApiPowerLimiterClass::onAdminPost, this, _1));
    server.on("/api/powerlimiter/metadata", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onMetaData, this, _1));
}

void WebApiPowerLimiterClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    ConfigurationClass::serializePowerLimiterConfig(config.PowerLimiter, root);
/*
    String buffer;
    serializeJsonPretty(root, buffer);
    Serial.println(buffer);
*/
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiPowerLimiterClass::onMetaData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    auto const& config = Configuration.get();

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["power_meter_enabled"] = config.PowerMeter.Enabled;
    root["battery_enabled"] = config.Battery.Enabled;
    root["charge_controller_enabled"] = config.Vedirect.Enabled;
#ifdef USE_CHARGER_MEANWELL
    root["charger_enabled"] = config.MeanWell.Enabled;
#endif
#ifdef USE_CHARGER_HUAWEI
    root["charger_enabled"] = config.Huawei.Enabled;
#endif

    auto inverters = root["inverters"].to<JsonArray>();
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);
        if (!inv) { continue; }

        auto obj = inverters.add<JsonObject>();
        obj["serial"] = inv->serialString();
        obj["pos"] = i;
        obj["order"] = config.Inverter[i].Order;
        obj["name"] = String(config.Inverter[i].Name);
        obj["poll_enable_day"] = config.Inverter[i].Poll_Enable_Day;
        obj["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        obj["command_enable_day"] = config.Inverter[i].Command_Enable_Day;
        obj["command_enable_night"] = config.Inverter[i].Command_Enable_Night;
        obj["max_power"] = inv->DevInfo()->getMaxPower(); // okay if zero/unknown
        obj["type"] = inv->typeName();
        auto channels = inv->Statistics()->getChannelsByType(TYPE_DC);
        obj["channels"] = channels.size();
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiPowerLimiterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onStatus(request);
}

void WebApiPowerLimiterClass::onAdminPost(AsyncWebServerRequest* request)
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

    // we were not actually checking for all the keys we (unconditionally)
    // access below for a long time, and it is technically not needed if users
    // use the web application to submit settings. the web app will always
    // submit all keys. users who send HTTP requests manually need to beware
    // anyways to always include the keys accessed below. if we wanted to
    // support a simpler API, like only sending the "enabled" key which only
    // changes that key, we need to refactor all of the code below.
    if (!root["enabled"].is<bool>()) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    PowerLimiter.setMode(PowerLimiterClass::Mode::Normal); // User input sets PL to normal operation
/*
    String buffer;
    serializeJsonPretty(root, buffer);
    Serial.println(buffer);
*/
    auto& config = Configuration.get();
    ConfigurationClass::deserializePowerLimiterConfig(root.as<JsonObject>(), config.PowerLimiter);

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    //PowerLimiter.calcNextInverterRestart();
    PowerLimiter.triggerReloadingConfig();
#ifdef USE_HASS
    // potentially make thresholds auto-discoverable
    MqttHandlePowerLimiterHass.forceUpdate();
#endif
}
