// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#ifdef CHARGER_HUAWEI

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

    response->setLength();
    request->send(response);
}

void WebApiHuaweiClass::onPost(AsyncWebServerRequest* request)
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

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);
    float value;
    uint8_t online = true;
    float minimal_voltage;

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (root.containsKey("online")) {
        online = root["online"].as<bool>();
        if (online) {
            minimal_voltage = HUAWEI_MINIMAL_ONLINE_VOLTAGE;
        } else {
            minimal_voltage = HUAWEI_MINIMAL_OFFLINE_VOLTAGE;
        }
    } else {
        retMsg["message"] = "Could not read info if data should be set for online/offline operation!";
        retMsg["code"] = WebApiError::LimitInvalidType;
        response->setLength();
        request->send(response);
        return;
    }

    if (root.containsKey("voltage_valid")) {
        if (root["voltage_valid"].as<bool>()) {
            if (root["voltage"].as<float>() < minimal_voltage || root["voltage"].as<float>() > 58) {
                retMsg["message"] = "voltage not in range between 42 (online)/48 (offline and 58V !";
                retMsg["code"] = WebApiError::LimitInvalidLimit;
                retMsg["param"]["max"] = 58;
                retMsg["param"]["min"] = minimal_voltage;
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["voltage"].as<float>();
                if (online) {
                    HuaweiCan.setValue(value, HUAWEI_ONLINE_VOLTAGE);
                } else {
                    HuaweiCan.setValue(value, HUAWEI_OFFLINE_VOLTAGE);
                }
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
                response->setLength();
                request->send(response);
                return;
            } else {
                value = root["current"].as<float>();
                if (online) {
                    HuaweiCan.setValue(value, HUAWEI_ONLINE_CURRENT);
                } else {
                    HuaweiCan.setValue(value, HUAWEI_OFFLINE_CURRENT);
                }
            }
        }
    }

    retMsg["type"] = Success;
    retMsg["message"] = SettingsSaved;
    retMsg["code"] = WebApiError::GenericSuccess;

    response->setLength();
    request->send(response);
}

void WebApiHuaweiClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const Huawei_CONFIG_T& cHuawei = Configuration.get().Huawei;

    root["enabled"] = cHuawei.Enabled;
    root["can_controller_frequency"] = cHuawei.CAN_Controller_Frequency;
    root["auto_power_enabled"] = cHuawei.Auto_Power_Enabled;
    root["voltage_limit"] = static_cast<int>(cHuawei.Auto_Power_Voltage_Limit * 100) / 100.0;
    root["enable_voltage_limit"] = static_cast<int>(cHuawei.Auto_Power_Enable_Voltage_Limit * 100) / 100.0;
    root["lower_power_limit"] = cHuawei.Auto_Power_Lower_Power_Limit;
    root["upper_power_limit"] = cHuawei.Auto_Power_Upper_Power_Limit;

    response->setLength();
    request->send(response);
}

void WebApiHuaweiClass::onAdminPost(AsyncWebServerRequest* request)
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

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled")) ||
        !(root.containsKey("can_controller_frequency")) ||
        !(root.containsKey("auto_power_enabled")) ||
        !(root.containsKey("voltage_limit")) ||
        !(root.containsKey("lower_power_limit")) ||
        !(root.containsKey("upper_power_limit"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    CONFIG_T& config = Configuration.get();
    config.Huawei.Enabled = root["enabled"].as<bool>();
    config.Huawei.CAN_Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.Huawei.Auto_Power_Enabled = root["auto_power_enabled"].as<bool>();
    config.Huawei.Auto_Power_Voltage_Limit = root["voltage_limit"].as<float>();
    config.Huawei.Auto_Power_Enable_Voltage_Limit = root["enable_voltage_limit"].as<float>();
    config.Huawei.Auto_Power_Lower_Power_Limit = root["lower_power_limit"].as<float>();
    config.Huawei.Auto_Power_Upper_Power_Limit = root["upper_power_limit"].as<float>();

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    // TODO(schlimmchen): HuaweiCan has no real concept of the fact that the
    // config might change. at least not regarding CAN parameters. until that
    // changes, the ESP must restart for configuration changes to take effect.
    yield();
    delay(1000);
    yield();
    ESP.restart();

    // Properly turn this on
    if (cHuawei.Enabled) {
      	HuaweiCan.init();
    }

    // Properly turn this off
    if (!cHuawei.Enabled) {
      HuaweiCan.setValue(0, HUAWEI_ONLINE_CURRENT);
      delay(500);
      HuaweiCan.setMode(HUAWEI_MODE_OFF);
      return;
    }

    if (cHuawei.Auto_Power_Enabled) {
      HuaweiCan.setMode(HUAWEI_MODE_AUTO_INT);
      return;
    }

    HuaweiCan.setMode(HUAWEI_MODE_AUTO_EXT);
}

#endif
