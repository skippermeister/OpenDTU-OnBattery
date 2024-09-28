// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#ifdef USE_CHARGER_HUAWEI

#include "WebApi_Huawei.h"
#include "Huawei_can.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

void WebApiHuaweiClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/charger/status", HTTP_GET, std::bind(&WebApiHuaweiClass::onStatus, this, _1));
    server.on("/api/charger/config", HTTP_GET, std::bind(&WebApiHuaweiClass::onAdminGet, this, _1));
    server.on("/api/charger/config", HTTP_POST, std::bind(&WebApiHuaweiClass::onAdminPost, this, _1));
    server.on("/api/huawei/limit/config", HTTP_POST, std::bind(&WebApiHuaweiClass::onPost, this, _1));
}

void WebApiHuaweiClass::getJsonData(JsonVariant& root) {
    const RectifierParameters_t * rp = HuaweiCan.get();

    root["data_age"] = (millis() - HuaweiCan.getLastUpdate()) / 1000;
    root["input_voltage"]["v"] = rp->input_voltage;
    root["input_voltage"]["u"] = "V";
    root["input_current"]["v"] = rp->input_current;
    root["input_current"]["u"] = "A";
    root["input_power"]["v"] = rp->input_power;
    root["input_power"]["u"] = "W";
    root["output_voltage"]["v"] = rp->output_voltage;
    root["output_voltage"]["u"] = "V";
    root["output_current"]["v"] = rp->output_current;
    root["output_current"]["u"] = "A";
    root["max_output_current"]["v"] = rp->max_output_current;
    root["max_output_current"]["u"] = "A";
    root["output_power"]["v"] = rp->output_power;
    root["output_power"]["u"] = "W";
    root["input_temp"]["v"] = rp->input_temp;
    root["input_temp"]["u"] = "°C";
    root["output_temp"]["v"] = rp->output_temp;
    root["output_temp"]["u"] = "°C";
    root["efficiency"]["v"] = rp->efficiency * 100;
    root["efficiency"]["u"] = "%";
}

void WebApiHuaweiClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    getJsonData(root);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiHuaweiClass::onPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    float value;
    uint8_t online = true;
    float minimal_voltage;

    auto& retMsg = response->getRoot();

    if (root["online"].is<bool>()) {
        online = root["online"].as<bool>();
        minimal_voltage = online ? HUAWEI_MINIMAL_ONLINE_VOLTAGE : HUAWEI_MINIMAL_OFFLINE_VOLTAGE;
    } else {
        retMsg["message"] = "Could not read info if data should be set for online/offline operation!";
        retMsg["code"] = WebApiError::LimitInvalidType;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["voltage_valid"].is<bool>()) {
        if (root["voltage_valid"].as<bool>()) {
            value = static_cast<int>(root["voltage"].as<float>() * 100) / 100.0;
            if (value < minimal_voltage || value > 58) {
                retMsg["message"] = "voltage not in range between 42 (online)/48 (offline and 58V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = 58;
                retMsg["param"]["min"] = minimal_voltage;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                HuaweiCan.setValue(value, online?HUAWEI_ONLINE_VOLTAGE:HUAWEI_OFFLINE_VOLTAGE);
            }
        }
    }

    if (root["current_valid"].is<bool>()) {
        if (root["current_valid"].as<bool>()) {
            value = static_cast<int>(root["current"].as<float>() * 100) / 100.0;
            if (value < 0 || value > 60) {
                retMsg["message"] = "current must be in range between 0 and 60!";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = 60;
                retMsg["param"]["min"] = 0;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                HuaweiCan.setValue(value, online?HUAWEI_ONLINE_CURRENT:HUAWEI_OFFLINE_CURRENT);
            }
        }
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiHuaweiClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& config = Configuration.get();

    root["enabled"] = config.Huawei.Enabled;
    root["verbose_logging"] = config.Huawei.VerboseLogging;
    root["chargerType"] = "Huawei R4850G2";
    root["io_providername"] = PinMapping.get().charger.providerName;
    if (HuaweiCanComm.isMCP2515Provider()) root["can_controller_frequency"] = config.MCP2515.Controller_Frequency;

    auto huawei = root["huawei"].to<JsonObject>();
    huawei["auto_power_enabled"]                   = config.Huawei.Auto_Power_Enabled;
    huawei["auto_power_batterysoc_limits_enabled"] = config.Huawei.Auto_Power_BatterySoC_Limits_Enabled;
    huawei["emergency_charge_enabled"]             = config.Huawei.Emergency_Charge_Enabled;
    huawei["voltage_limit"]                        = static_cast<int>(config.Huawei.Auto_Power_Voltage_Limit * 100 + 0.5) / 100.0;
    huawei["enable_voltage_limit"]                 = static_cast<int>(config.Huawei.Auto_Power_Enable_Voltage_Limit * 100 + 0.5) / 100.0;
    huawei["lower_power_limit"]                    = config.Huawei.Auto_Power_Lower_Power_Limit;
    huawei["upper_power_limit"]                    = config.Huawei.Auto_Power_Upper_Power_Limit;
    huawei["stop_batterysoc_threshold"]            = config.Huawei.Auto_Power_Stop_BatterySoC_Threshold;
    huawei["target_power_consumption"]             = config.Huawei.Auto_Power_Target_Power_Consumption;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiHuaweiClass::onAdminPost(AsyncWebServerRequest* request)
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
        !root["verbose_logging"].is<bool>() ||
        !root["huawei"]["auto_power_enabled"].is<bool>() ||
        !root["huawei"]["emergency_charge_enabled"].is<bool>() ||
        !root["huawei"]["voltage_limit"].is<float>() ||
        !root["huawei"]["lower_power_limit"].is<float>() ||
        !root["huawei"]["upper_power_limit"].is<float>())
    {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.Huawei.Enabled = root["enabled"];
    config.Huawei.VerboseLogging = root["verbose_logging"];
    if (HuaweiCanComm.isMCP2515Provider()) config.MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.Huawei.Auto_Power_Enabled                   = root["huawei"]["auto_power_enabled"];
    config.Huawei.Auto_Power_BatterySoC_Limits_Enabled = root["huawei"]["auto_power_batterysoc_limits_enabled"];
    config.Huawei.Emergency_Charge_Enabled             = root["huawei"]["emergency_charge_enabled"];
    config.Huawei.Auto_Power_Voltage_Limit             = static_cast<int>(root["huawei"]["voltage_limit"].as<float>() * 100) / 100.0;
    config.Huawei.Auto_Power_Enable_Voltage_Limit      = static_cast<int>(root["huawei"]["enable_voltage_limit"].as<float>() * 100) / 100.0;
    config.Huawei.Auto_Power_Lower_Power_Limit         = root["huawei"]["lower_power_limit"].as<float>();
    config.Huawei.Auto_Power_Upper_Power_Limit         = root["huawei"]["upper_power_limit"].as<float>();
    config.Huawei.Auto_Power_Stop_BatterySoC_Threshold = root["huawei"]["stop_batterysoc_threshold"];
    config.Huawei.Auto_Power_Target_Power_Consumption  = root["huawei"]["target_power_consumption"];

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    // TODO(schlimmchen): HuaweiCan has no real concept of the fact that the
    // config might change. at least not regarding CAN parameters. until that
    // changes, the ESP must restart for configuration changes to take effect.
    yield();
    delay(1000);
    yield();
    ESP.restart();

    // Properly turn this on
    if (config.Huawei.Enabled) {
        MessageOutput.println("Initialize Huawei AC charger interface... ");
        HuaweiCan.updateSettings();
    }

    // Properly turn this off
    if (!config.Huawei.Enabled) {
      HuaweiCan.setValue(0, HUAWEI_ONLINE_CURRENT);
      delay(500);
      HuaweiCan.setMode(HUAWEI_MODE_OFF);
      return;
    }

    if (config.Huawei.Auto_Power_Enabled) {
      HuaweiCan.setMode(HUAWEI_MODE_AUTO_INT);
      return;
    }

    HuaweiCan.setMode(HUAWEI_MODE_AUTO_EXT);
}

#endif
