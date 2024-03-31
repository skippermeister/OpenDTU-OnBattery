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

    server.on("/api/zeroexport/status", HTTP_GET, std::bind(&WebApiZeroExportClass::onStatus, this, _1));
    server.on("/api/zeroexport/config", HTTP_GET, std::bind(&WebApiZeroExportClass::onAdminGet, this, _1));
    server.on("/api/zeroexport/config", HTTP_POST, std::bind(&WebApiZeroExportClass::onAdminPost, this, _1));
    server.on("/api/zeroexport/metadata", HTTP_GET, std::bind(&WebApiZeroExportClass::onMetaData, this, _1));
}

void WebApiZeroExportClass::onStatus(AsyncWebServerRequest* request)
{
    const ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root["enabled"] = cZeroExport.Enabled;
    root["updatesonly"] = cZeroExport.UpdatesOnly;
    root["verbose_logging"] = ZeroExport.getVerboseLogging();
//    root["InverterId"] = cZeroExport.InverterId; // the bid mask of the inverter Ids
    JsonArray serials = root.createNestedArray("serials");
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (cZeroExport.serials[i] != 0) {
            serials[i] = String(cZeroExport.serials[i]);
        }
    }
    root["MaxGrid"] = cZeroExport.MaxGrid;
    root["MinimumLimit"] = cZeroExport.MinimumLimit;
    root["PowerHysteresis"] = cZeroExport.PowerHysteresis;
    root["Tn"] = cZeroExport.Tn;

    {
        String buffer;
        serializeJsonPretty(root, buffer);
        Serial.println(buffer);
    }

    response->setLength();
    request->send(response);
}

void WebApiZeroExportClass::onMetaData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    auto const& config = Configuration.get();

    size_t invAmount = 0;
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial != 0) { ++invAmount; }
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, 128 + 256 * invAmount);
    auto& root = response->getRoot();

    root["powerlimiter_inverter_serial"] = config.PowerLimiter.InverterId;

    JsonObject inverters = root.createNestedObject("inverters");
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == 0) { continue; }

        // we use the integer (base 10) representation of the inverter serial,
        // rather than the hex represenation as used when handling the inverter
        // serial elsewhere in the web application, because in this case, the
        // serial is actually not displayed but only used as a value/index.
        JsonObject obj = inverters.createNestedObject(String(config.Inverter[i].Serial));
        obj["pos"] = i;
        obj["name"] = String(config.Inverter[i].Name);
/*
        obj["poll_enable_day"] = config.Inverter[i].Poll_Enable_Day;
        obj["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        obj["command_enable_day"] = config.Inverter[i].Command_Enable_Day;
        obj["command_enable_night"] = config.Inverter[i].Command_Enable_Night;
*/
        obj["type"] = "Unknown";
        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);
        if (inv != nullptr) { obj["type"] = inv->typeName(); }
        obj["selected"] = false;
        for (uint8_t j=0; j<INV_MAX_COUNT; j++) {
            if (config.Inverter[i].Serial == config.ZeroExport.serials[j]) {
                obj["selected"] = true;
                break;
            }
        }
    }

    {
        String buffer;
        serializeJsonPretty(root, buffer);
        MessageOutput.println(buffer);
    }

    response->setLength();
    request->send(response);
}

void WebApiZeroExportClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

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
//            && root.containsKey("InverterId")
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

    {
        String buffer;
        serializeJsonPretty(root, buffer);
        MessageOutput.println(buffer);
    }

    ZeroExport_CONFIG_T& cZeroExport = Configuration.get().ZeroExport;
    ZeroExport.setVerboseLogging(root["verbose_logging"].as<bool>());
    cZeroExport.Enabled = root["enabled"].as<bool>();
    cZeroExport.UpdatesOnly = root["updatesonly"].as<bool>();
    cZeroExport.MaxGrid = root["MaxGrid"].as<uint16_t>();
    cZeroExport.PowerHysteresis = root["PowerHysteresis"].as<uint16_t>();
//    cZeroExport.InverterId = root["InverterId"].as<uint16_t>();

    if (root.containsKey("serials")) {
        JsonArray serials = root["serials"].as<JsonArray>();
        if (serials.size() > 0 && serials.size() <= INV_MAX_COUNT) {
            for (uint8_t i=0; i< INV_MAX_COUNT; i++) { cZeroExport.serials[i] = 0; }
            uint8_t serialCount = 0;
            for (JsonVariant serial : serials) {
                cZeroExport.serials[serialCount] = serial.as<uint64_t>();
                if (ZeroExport.getVerboseLogging())
                    MessageOutput.printf("[WebApiZeroExport]%s: Serial No: %" PRIx64 "\r\n", __FUNCTION__, cZeroExport.serials[serialCount]);
                serialCount++;
            }
        }
    }

    cZeroExport.MinimumLimit = root["MinimumLimit"].as<uint16_t>();
    cZeroExport.Tn = root["Tn"].as<uint16_t>();

    Configuration.write();

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);
}
