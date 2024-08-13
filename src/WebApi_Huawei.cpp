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

    server.on("/api/huawei/status", HTTP_GET, std::bind(&WebApiHuaweiClass::onStatus, this, _1));
    server.on("/api/huawei/config", HTTP_GET, std::bind(&WebApiHuaweiClass::onAdminGet, this, _1));
    server.on("/api/huawei/config", HTTP_POST, std::bind(&WebApiHuaweiClass::onAdminPost, this, _1));
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

    if (root.containsKey("online")) {
        online = root["online"].as<bool>();
        minimal_voltage = online ? HUAWEI_MINIMAL_ONLINE_VOLTAGE : HUAWEI_MINIMAL_OFFLINE_VOLTAGE;
    } else {
        retMsg["message"] = "Could not read info if data should be set for online/offline operation!";
        retMsg["code"] = WebApiError::LimitInvalidType;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root.containsKey("voltage_valid")) {
        if (root["voltage_valid"].as<bool>()) {
            if (root["voltage"].as<float>() < minimal_voltage || root["voltage"].as<float>() > 58) {
                retMsg["message"] = "voltage not in range between 42 (online)/48 (offline and 58V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = 58;
                retMsg["param"]["min"] = minimal_voltage;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                value = root["voltage"].as<float>();
                HuaweiCan.setValue(value, online?HUAWEI_ONLINE_VOLTAGE:HUAWEI_OFFLINE_VOLTAGE);
            }
        }
    }

    if (root.containsKey("current_valid")) {
        if (root["current_valid"].as<bool>()) {
            if (root["current"].as<float>() < 0 || root["current"].as<float>() > 60) {
                retMsg["message"] = "current must be in range between 0 and 60!";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = 60;
                retMsg["param"]["min"] = 0;
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            } else {
                value = root["current"].as<float>();
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
    const Huawei_CONFIG_T& cHuawei = Configuration.get().Huawei;

    root["enabled"] = cHuawei.Enabled;
    root["verbose_logging"] = cHuawei.VerboseLogging;
    if (HuaweiCanComm.isMCP2515Provider()) root["can_controller_frequency"] = Configuration.get().MCP2515.Controller_Frequency;
    root["auto_power_enabled"] = cHuawei.Auto_Power_Enabled;
    root["auto_power_batterysoc_limits_enabled"] = cHuawei.Auto_Power_BatterySoC_Limits_Enabled;
    root["emergency_charge_enabled"] = cHuawei.Emergency_Charge_Enabled;
    root["voltage_limit"] = static_cast<int>(cHuawei.Auto_Power_Voltage_Limit * 100) / 100.0;
    root["enable_voltage_limit"] = static_cast<int>(cHuawei.Auto_Power_Enable_Voltage_Limit * 100) / 100.0;
    root["lower_power_limit"] = cHuawei.Auto_Power_Lower_Power_Limit;
    root["upper_power_limit"] = cHuawei.Auto_Power_Upper_Power_Limit;
    root["stop_batterysoc_threshold"] = cHuawei.Auto_Power_Stop_BatterySoC_Threshold;
    root["target_power_consumption"] = cHuawei.Auto_Power_Target_Power_Consumption;

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

    if (!(root.containsKey("enabled")) ||
        !(root.containsKey("verbose_logging")) ||
        !(root.containsKey("auto_power_enabled")) ||
        !(root.containsKey("emergency_charge_enabled")) ||
        !(root.containsKey("voltage_limit")) ||
        !(root.containsKey("lower_power_limit")) ||
        !(root.containsKey("upper_power_limit")))
    {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    CONFIG_T& config = Configuration.get();
    config.Huawei.Enabled = root["enabled"].as<bool>();
    config.Huawei.VerboseLogging = root["verbose_logging"];
    if (HuaweiCanComm.isMCP2515Provider()) config.MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.Huawei.Auto_Power_Enabled = root["auto_power_enabled"].as<bool>();
    config.Huawei.Auto_Power_BatterySoC_Limits_Enabled = root["auto_power_batterysoc_limits_enabled"].as<bool>();
    config.Huawei.Emergency_Charge_Enabled = root["emergency_charge_enabled"].as<bool>();
    config.Huawei.Auto_Power_Voltage_Limit = root["voltage_limit"].as<float>();
    config.Huawei.Auto_Power_Enable_Voltage_Limit = root["enable_voltage_limit"].as<float>();
    config.Huawei.Auto_Power_Lower_Power_Limit = root["lower_power_limit"].as<float>();
    config.Huawei.Auto_Power_Upper_Power_Limit = root["upper_power_limit"].as<float>();
    config.Huawei.Auto_Power_Stop_BatterySoC_Threshold = root["stop_batterysoc_threshold"];
    config.Huawei.Auto_Power_Target_Power_Consumption = root["target_power_consumption"];

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
