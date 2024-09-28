// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Andreas Högner
 */
#ifdef USE_CHARGER_MEANWELL

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

void WebApiMeanWellClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/charger/status", HTTP_GET, std::bind(&WebApiMeanWellClass::onStatus, this, _1));
    server.on("/api/charger/config", HTTP_GET, std::bind(&WebApiMeanWellClass::onAdminGet, this, _1));
    server.on("/api/charger/config", HTTP_POST, std::bind(&WebApiMeanWellClass::onAdminPost, this, _1));

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

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiMeanWellClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& config = Configuration.get();

    root["enabled"] = config.MeanWell.Enabled;
    root["verbose_logging"] = config.MeanWell.VerboseLogging;
    root["updatesonly"] = config.MeanWell.UpdatesOnly;
    root["chargerType"] = (strcmp(MeanWellCan._rp.ManufacturerName, "MEANWELL") == 0) ?
            String(MeanWellCan._rp.ManufacturerName) + " " + String(MeanWellCan._rp.ManufacturerModelName):
            "MEANWELL NPB-450/750/1200/1700-24/48";
    root["io_providername"] = PinMapping.get().charger.providerName;
    if (MeanWellCan.isMCP2515Provider()) root["can_controller_frequency"] = config.MCP2515.Controller_Frequency;

    auto meanwell = root["meanwell"].to<JsonObject>();
    meanwell["pollinterval"] = config.MeanWell.PollInterval;
    meanwell["min_voltage"]  = static_cast<int>(config.MeanWell.MinVoltage * 100.0 + 0.5) / 100.0;
    meanwell["max_voltage"]  = static_cast<int>(config.MeanWell.MaxVoltage * 100.0 + 0.5) / 100.0;
    meanwell["min_current"]  = static_cast<int>(config.MeanWell.MinCurrent * 100.0 + 0.5) / 100.0;
    meanwell["max_current"]  = static_cast<int>(config.MeanWell.MaxCurrent * 100.0 + 0.5) / 100.0;
    meanwell["hysteresis"]   = config.MeanWell.Hysteresis;
    meanwell["EEPROMwrites"] = MeanWellCan.getEEPROMwrites();
    meanwell["mustInverterProduce"] = config.MeanWell.mustInverterProduce;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiMeanWellClass::onAdminPost(AsyncWebServerRequest* request)
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
        !root["verbose_logging"].is<bool>() ||
        !root["meanwell"]["pollinterval"].is<uint32_t>() ||
        !root["meanwell"]["min_voltage"].is<float>() ||
        !root["meanwell"]["max_voltage"].is<float>() ||
        !root["meanwell"]["min_current"].is<float>() ||
        !root["meanwell"]["max_current"].is<float>() ||
        !root["meanwell"]["hysteresis"].is<float>())
    {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["meanwell"]["pollinterval"].as<uint32_t>() == 0) {
        retMsg["message"] = "Poll interval must be a number between 5 and 65535!";
        retMsg["code"] = WebApiError::MqttPublishInterval;
        retMsg["param"]["min"] = 5;
        retMsg["param"]["max"] = 65535;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.MeanWell.Enabled        = root["enabled"];
    config.MeanWell.VerboseLogging = root["verbose_logging"];
    config.MeanWell.UpdatesOnly    = root["updatesonly"];
    if (MeanWellCan.isMCP2515Provider()) config.MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.MeanWell.PollInterval   = root["meanwell"]["pollinterval"].as<uint32_t>();
    config.MeanWell.MinVoltage     = static_cast<int>(root["meanwell"]["min_voltage"].as<float>() * 100) / 100.0;
    config.MeanWell.MaxVoltage     = static_cast<int>(root["meanwell"]["max_voltage"].as<float>() * 100) / 100.0;
    config.MeanWell.MinCurrent     = static_cast<int>(root["meanwell"]["min_current"].as<float>() * 100) / 100.0;
    config.MeanWell.MaxCurrent     = static_cast<int>(root["meanwell"]["max_current"].as<float>() * 100) / 100.0;
    config.MeanWell.Hysteresis     = root["meanwell"]["hysteresis"].as<float>();
    config.MeanWell.mustInverterProduce = root["meanwell"]["mustInverterProduce"];

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

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

    root["voltage"] = static_cast<int>(MeanWellCan._rp.outputVoltageSet * 100 + 0.5) / 100.0;
    root["current"] = static_cast<int>(MeanWellCan._rp.outputCurrentSet * 100 + 0.5) / 100.0;
    root["curveCV"] = static_cast<int>(MeanWellCan._rp.curveCV * 100 + 0.5) / 100.0;
    root["curveCC"] = static_cast<int>(MeanWellCan._rp.curveCC * 100 + 0.5) / 100.0;
    root["curveFV"] = static_cast<int>(MeanWellCan._rp.curveFV * 100 + 0.5) / 100.0;
    root["curveTC"] = static_cast<int>(MeanWellCan._rp.curveTC * 100 + 0.5) / 100.0;

    /*    if (Configuration.get().MeanWell.VerboseLogging) {
            String output;
            serializeJsonPretty(root, output);
            MessageOutput.println(output);
        }
    */
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiMeanWellClass::onLimitPost(AsyncWebServerRequest* request)
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

    /*
        if (Configuration.get().MeanWell.VerboseLogging) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */
    float value;

    auto const& cMeanWell = Configuration.get().MeanWell;

    if (root["voltageValid"].is<bool>()) {
        if (root["voltageValid"].as<bool>()) {
            value = static_cast<int>(root["voltage"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.VoltageLimitMin || value > cMeanWell.VoltageLimitMax) {
                retMsg["message"] = "voltage not in range between " + String(cMeanWell.VoltageLimitMin,0) + "V and " + String(cMeanWell.VoltageLimitMax,0) + "V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.VoltageLimitMax;
                retMsg["param"]["min"] = cMeanWell.VoltageLimitMin;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_VOLTAGE);
            }
        }
    }

    if (root["currentValid"].is<bool>()) {
        if (root["currentValid"].as<bool>()) {
            value = static_cast<int>(root["current"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.CurrentLimitMin || value > cMeanWell.CurrentLimitMax) {
                retMsg["message"] = "current must be in range between " + String(cMeanWell.CurrentLimitMin,2) + "A and " + String(cMeanWell.CurrentLimitMax,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.CurrentLimitMax;
                retMsg["param"]["min"] = cMeanWell.CurrentLimitMin;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_CURRENT);
            }
        }
    }

    if (root["curveCVvalid"].is<bool>()) {
        if (root["curveCVvalid"].as<bool>()) {
            value = static_cast<int>(root["curveCV"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.VoltageLimitMin || value > cMeanWell.VoltageLimitMax) {
                retMsg["message"] = "voltage not in range between " + String(cMeanWell.VoltageLimitMin,0) + "V and " + String(cMeanWell.VoltageLimitMax,0) +"V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.VoltageLimitMax;
                retMsg["param"]["min"] = cMeanWell.VoltageLimitMin;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_CV);
            }
        }
    }

    if (root["curveCCvalid"].is<bool>()) {
        if (root["curveCCvalid"].as<bool>()) {
            value = static_cast<int>(root["curveCC"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.CurrentLimitMin || value > cMeanWell.CurrentLimitMax) {
                retMsg["message"] = "Curve constant current must be in range between " + String(cMeanWell.CurrentLimitMin,2) + "A and " + String(cMeanWell.CurrentLimitMax,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.CurrentLimitMax;
                retMsg["param"]["min"] = cMeanWell.CurrentLimitMin;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_CC);
            }
        }
    }

    if (root["curveFVvalid"].is<bool>()) {
        if (root["curveFVvalid"].as<bool>()) {
            value = static_cast<int>(root["curveFV"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.VoltageLimitMin || value > cMeanWell.VoltageLimitMax) {
                retMsg["message"] = "Curve float voltage not in range between " + String(cMeanWell.VoltageLimitMin,0) + "V and " + String(cMeanWell.VoltageLimitMax,0) + "V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.VoltageLimitMax;
                retMsg["param"]["min"] = cMeanWell.VoltageLimitMin;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_FV);
            }
        }
    }

    if (root["curveTCvalid"].is<bool>()) {
        if (root["curveTCvalid"].as<bool>()) {
            value = static_cast<int>(root["curveTC"].as<float>() * 100) / 100.0;
            if (value < cMeanWell.CurrentLimitMin / 10.0 || value > cMeanWell.CurrentLimitMax / 3.33333333) {
                retMsg["message"] = "Taper constant current must be in range between " + String(cMeanWell.CurrentLimitMin/10,2) + "A and " + String(cMeanWell.CurrentLimitMax/3.3333,2) + "A !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = cMeanWell.CurrentLimitMax / 3.33333333;
                retMsg["param"]["min"] = cMeanWell.CurrentLimitMin / 10.0;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                MeanWellCan.setValue(value, MEANWELL_SET_CURVE_TC);
            }
        }
    }

    retMsg["type"] = "success";
    retMsg["message"] = "Settings saved!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
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
        if (Configuration.get().MeanWell.VerboseLogging) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiMeanWellClass::onPowerPost(AsyncWebServerRequest* request)
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

    /*
        if (Configuration.get().MeanWell.VerboseLogging) {
            String output;
            serializeJson(root, output);
            MessageOutput.println(output);
        }
    */

    if (root["power"].is<uint16_t>()) {
        uint16_t power = root["power"].as<uint16_t>();

        if (Configuration.get().MeanWell.VerboseLogging)
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

    retMsg["type"] = "success";
    retMsg["message"] = "Settings saved!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

#endif
