// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Andreas Högner
 */
#ifndef CHARGER_HUAWEI

#include "WebApi_MeanWell.h"
#include "Configuration.h"
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "Scheduler.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>
#include "Utils.h"

void WebApiMeanWellClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/meanwell/status", HTTP_GET, std::bind(&WebApiMeanWellClass::onStatus, this, _1));
    server.on("/api/meanwell/config", HTTP_GET, std::bind(&WebApiMeanWellClass::onAdminGet, this, _1));
    server.on("/api/meanwell/config", HTTP_POST, std::bind(&WebApiMeanWellClass::onAdminPost, this, _1));

    server.on("/api/meanwell/limit/config", HTTP_GET, std::bind(&WebApiMeanWellClass::onLimitGet, this, _1));
    server.on("/api/meanwell/limit/config", HTTP_POST, std::bind(&WebApiMeanWellClass::onLimitPost, this, _1));

    server.on("/api/meanwell/power/config", HTTP_GET, std::bind(&WebApiMeanWellClass::onPowerGet, this, _1));
    server.on("/api/meanwell/power/config", HTTP_POST, std::bind(&WebApiMeanWellClass::onPowerPost, this, _1));
}

void WebApiMeanWellClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    MeanWellCan.generateJsonResponse(root);

    if (Utils::checkJsonOverflow(root, __FUNCTION__, __LINE__)) { return; }

    response->setLength();
    request->send(response);
}

void WebApiMeanWellClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;

    root["enabled"] = cMeanWell.Enabled;
    root["pollinterval"] = cMeanWell.PollInterval;
    root["updatesonly"] = cMeanWell.UpdatesOnly;
    root["min_voltage"] = cMeanWell.MinVoltage;
    root["max_voltage"] = cMeanWell.MaxVoltage;
    root["min_current"] = cMeanWell.MinCurrent;
    root["max_current"] = cMeanWell.MaxCurrent;
    root["hysteresis"] = cMeanWell.Hysteresis;
    root["verbose_logging"] = MeanWellCan.getVerboseLogging();
    root["EEPROMwrites"] = MeanWellCan.getEEPROMwrites();
    root["mustInverterProduce"] = cMeanWell.mustInverterProduce;

    response->setLength();
    request->send(response);
}

void WebApiMeanWellClass::onAdminPost(AsyncWebServerRequest* request)
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
            && root.containsKey("min_voltage")
            && root.containsKey("max_voltage")
            && root.containsKey("min_current")
            && root.containsKey("max_current")
            && root.containsKey("hysteresis")
            && root.containsKey("verbose_logging"))) {
        retMsg["message"] = ValuesAreMissing;
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

    MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;
    cMeanWell.Enabled = root["enabled"].as<bool>();
    cMeanWell.UpdatesOnly = root["updatesonly"].as<bool>();
    cMeanWell.PollInterval = root["pollinterval"].as<uint32_t>();
    cMeanWell.MinVoltage = root["min_voltage"].as<float>();
    cMeanWell.MaxVoltage = root["max_voltage"].as<float>();
    cMeanWell.MinCurrent = root["min_current"].as<float>();
    cMeanWell.MaxCurrent = root["max_current"].as<float>();
    cMeanWell.Hysteresis = root["hysteresis"].as<float>();
    cMeanWell.mustInverterProduce = root["mustInverterProduce"].as<bool>();
    MeanWellCan.setVerboseLogging(root["verbose_logging"].as<bool>());

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    MeanWellCan.updateSettings();
}

void WebApiMeanWellClass::onLimitGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    // MeanWellCan.generateJsonResponse(root);

    root["voltage"] = MeanWellCan._rp.outputVoltageSet;
    root["current"] = MeanWellCan._rp.outputCurrentSet;
    root["curveCV"] = MeanWellCan._rp.curveCV;
    root["curveCC"] = MeanWellCan._rp.curveCC;
    root["curveFV"] = MeanWellCan._rp.curveFV;
    root["curveTC"] = MeanWellCan._rp.curveTC;

    /*    if (MeanWellCan.getVerboseLogging()) {
            String output;
            serializeJsonPretty(root, output);
            MessageOutput.println(output);
        }
    */
    response->setLength();
    request->send(response);
}

void WebApiMeanWellClass::onLimitPost(AsyncWebServerRequest* request)
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

    /*
        if (MeanWellCan.getVerboseLogging()) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */
    float value;

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;

    if (root.containsKey("voltageValid")) {
        if (root["voltageValid"].as<bool>()) {
            if (root["voltage"].as<float>() < cMeanWell.MinVoltage
                || root["voltage"].as<float>() > cMeanWell.MaxVoltage) {
                retMsg["message"] = "voltage not in range between " + String(cMeanWell.MinVoltage,0) + "V and " + String(cMeanWell.MaxVoltage,0) + "V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxVoltage;
                retMsg["param"]["min"] = cMeanWell.MinVoltage;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["voltage"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_VOLTAGE);
            }
        }
    }

    if (root.containsKey("currentValid")) {
        if (root["currentValid"].as<bool>()) {
            if (root["current"].as<float>() < cMeanWell.MinCurrent
                || root["current"].as<float>() > cMeanWell.MaxCurrent) {
                retMsg["message"] = "current must be in range between " + String(cMeanWell.MinCurrent,2) + "A and " + String(cMeanWell.MaxCurrent,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxCurrent;
                retMsg["param"]["min"] = cMeanWell.MinCurrent;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["current"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_CURRENT);
            }
        }
    }

    if (root.containsKey("curveCVvalid")) {
        if (root["curveCVvalid"].as<bool>()) {
            if (root["curveCV"].as<float>() < cMeanWell.MinVoltage
                || root["curveCV"].as<float>() > cMeanWell.MaxVoltage) {
                retMsg["message"] = "voltage not in range between " + String(cMeanWell.MinVoltage,0) + "V and " + String(cMeanWell.MaxVoltage,0) +"V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxVoltage;
                retMsg["param"]["min"] = cMeanWell.MinVoltage;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["curveCV"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_CV);
            }
        }
    }

    if (root.containsKey("curveCCvalid")) {
        if (root["curveCCvalid"].as<bool>()) {
            if (root["curveCC"].as<float>() < cMeanWell.MinCurrent
                || root["curveCC"].as<float>() > cMeanWell.MaxCurrent) {
                retMsg["message"] = "Curve constant current must be in range between " + String(cMeanWell.MinCurrent,2) + "A and " + String(cMeanWell.MaxCurrent,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxCurrent;
                retMsg["param"]["min"] = cMeanWell.MinCurrent;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["curveCC"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_CC);
            }
        }
    }

    if (root.containsKey("curveFVvalid")) {
        if (root["curveFVvalid"].as<bool>()) {
            if (root["curveFV"].as<float>() < cMeanWell.MinVoltage
                || root["curveFV"].as<float>() > cMeanWell.MaxVoltage) {
                retMsg["message"] = "Curve float voltage not in range between " + String(cMeanWell.MinVoltage,0) + "V and " + String(cMeanWell.MaxVoltage,0) + "V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxVoltage;
                retMsg["param"]["min"] = cMeanWell.MinVoltage;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["curveFV"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_FV);
            }
        }
    }

    if (root.containsKey("curveTCvalid")) {
        if (root["curveTCvalid"].as<bool>()) {
            if (root["curveTC"].as<float>() < cMeanWell.MinCurrent / 10.0
                || root["curveTC"].as<float>() > cMeanWell.MaxCurrent / 3.33333333) {
                retMsg["message"] = "Taper constant current must be in range between " + String(cMeanWell.MinCurrent/10,2) + "A and " + String(cMeanWell.MaxCurrent/3.3333,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.MaxCurrent / 3.33333333;
                retMsg["param"]["min"] = cMeanWell.MinCurrent / 10.0;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["curveTC"].as<float>();
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_TC);
            }
        }
    }

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);
}

void WebApiMeanWellClass::onPowerGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["power_set_status"] = MeanWellCan.getLastPowerCommandSuccess() ? "Ok" : "Failure";

    /*
        if (MeanWellCan.getVerboseLogging()) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */
    response->setLength();
    request->send(response);
}

void WebApiMeanWellClass::onPowerPost(AsyncWebServerRequest* request)
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

    /*
        if (MeanWellCan.getVerboseLogging()) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */
    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (root.containsKey("power")) {
        uint16_t power = root["power"].as<int>();

        if (MeanWellCan.getVerboseLogging())
            MessageOutput.printf("Power: %s\r\n",
                power == 0 ? "off" : power == 1 ? "on"
                    : power == 2                ? "auto"
                                                : "unknown");

        if (power == 1) {
            MeanWellCan.setAutomaticChargeMode(false);
            MeanWellCan.setPower(true);
        } else if (power == 2) {
            MeanWellCan.setAutomaticChargeMode(true);
        } else if (power == 0) {
            MeanWellCan.setAutomaticChargeMode(false);
            MeanWellCan.setPower(false);
        }
    }

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);
}

#endif
