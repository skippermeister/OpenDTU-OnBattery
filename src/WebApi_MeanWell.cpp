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
    root["chargerType"] = (strcmp(MeanWellCan._rp.ManufacturerName, "MEANWELL") == 0) ?
            String(MeanWellCan._rp.ManufacturerName) + " " + String(MeanWellCan._rp.ManufacturerModelName):
            "MeanWell NPB-450/750/1200/1700-24/48";
    root["io_providername"] = PinMapping.get().charger.providerName;
    if (MeanWellCan.isMCP2515Provider()) root["can_controller_frequency"] = config.MCP2515.Controller_Frequency;
    root["pollinterval"] = config.MeanWell.PollInterval;
    root["updatesonly"] = config.MeanWell.UpdatesOnly;
    root["min_voltage"] = config.MeanWell.MinVoltage;
    root["max_voltage"] = config.MeanWell.MaxVoltage;
    root["min_current"] = config.MeanWell.MinCurrent;
    root["max_current"] = config.MeanWell.MaxCurrent;
    root["hysteresis"] = config.MeanWell.Hysteresis;
    root["EEPROMwrites"] = MeanWellCan.getEEPROMwrites();
    root["mustInverterProduce"] = config.MeanWell.mustInverterProduce;

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
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["pollinterval"].as<uint32_t>() == 0) {
        retMsg["message"] = "Poll interval must be a number between 5 and 65535!";
        retMsg["code"] = WebApiError::MqttPublishInterval;
        retMsg["param"]["min"] = 5;
        retMsg["param"]["max"] = 65535;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.MeanWell.Enabled = root["enabled"].as<bool>();
    config.MeanWell.VerboseLogging = root["verbose_logging"].as<bool>();
    config.MeanWell.UpdatesOnly = root["updatesonly"].as<bool>();
    if (MeanWellCan.isMCP2515Provider()) config.MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.MeanWell.PollInterval = root["pollinterval"].as<uint32_t>();
    config.MeanWell.MinVoltage = root["min_voltage"].as<float>();
    config.MeanWell.MaxVoltage = root["max_voltage"].as<float>();
    config.MeanWell.MinCurrent = root["min_current"].as<float>();
    config.MeanWell.MaxCurrent = root["max_current"].as<float>();
    config.MeanWell.Hysteresis = root["hysteresis"].as<float>();
    config.MeanWell.mustInverterProduce = root["mustInverterProduce"].as<bool>();

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

    root["voltage"] = MeanWellCan._rp.outputVoltageSet;
    root["current"] = MeanWellCan._rp.outputCurrentSet;
    root["curveCV"] = MeanWellCan._rp.curveCV;
    root["curveCC"] = MeanWellCan._rp.curveCC;
    root["curveFV"] = MeanWellCan._rp.curveFV;
    root["curveTC"] = MeanWellCan._rp.curveTC;

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

    if (root.containsKey("voltageValid")) {
        if (root["voltageValid"].as<bool>()) {
            value = root["voltage"].as<float>();
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

    if (root.containsKey("currentValid")) {
        if (root["currentValid"].as<bool>()) {
            value = root["current"].as<float>();
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

    if (root.containsKey("curveCVvalid")) {
        if (root["curveCVvalid"].as<bool>()) {
            value = root["curveCV"].as<float>();
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

    if (root.containsKey("curveCCvalid")) {
        if (root["curveCCvalid"].as<bool>()) {
            value = root["curveCC"].as<float>();
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

    if (root.containsKey("curveFVvalid")) {
        if (root["curveFVvalid"].as<bool>()) {
            value = root["curveFV"].as<float>();
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

    if (root.containsKey("curveTCvalid")) {
        if (root["curveTCvalid"].as<bool>()) {
            value = root["curveTC"].as<float>();
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

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
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

    if (root.containsKey("power")) {
        uint16_t power = root["power"].as<int>();

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

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

#endif
