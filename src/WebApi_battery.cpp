// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */

#include "WebApi_battery.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Battery.h"
#include "Configuration.h"
#include "MqttHandleBatteryHass.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "defaults.h"
#include "helper.h"

void WebApiBatteryClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/battery/status", HTTP_GET, std::bind(&WebApiBatteryClass::onStatus, this, _1));
    server.on("/api/battery/config", HTTP_GET, std::bind(&WebApiBatteryClass::onAdminGet, this, _1));
    server.on("/api/battery/config", HTTP_POST, std::bind(&WebApiBatteryClass::onAdminPost, this, _1));
}

void WebApiBatteryClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    root["enabled"] = cBattery.Enabled;
    root["numberOfBatteries"] = cBattery.numberOfBatteries;
    root["pollinterval"] = cBattery.PollInterval;
    root["provider"] = cBattery.Provider;
    root["can_controller_frequency"] = Configuration.get().MCP2515.Controller_Frequency;
#ifdef USE_JKBMS_CONTROLLER
    root["jkbms_interface"] = cBattery.JkBms.Interface;
    root["jkbms_polling_interval"] = root["pollinterval"] = cBattery.PollInterval; // cBattery.JkBms.PollingInterval;
#else
    root["jkbms_interface"] = 0;
    root["jkbms_polling_interval"] = 0;
#endif
#ifdef USE_MQTT_BATTERY
    root["mqtt_soc_topic"] = config.Battery.Mqtt.SocTopic;
    root["mqtt_voltage_topic"] = config.Battery.Mqtt.VoltageTopic;
#else
    root["mqtt_soc_topic"] = "";
    root["mqtt_voltage_topic"] = "";
#endif
    root["updatesonly"] = cBattery.UpdatesOnly;
    root["min_charge_temp"] = cBattery.MinChargeTemperature;
    root["max_charge_temp"] = cBattery.MaxChargeTemperature;
    root["min_discharge_temp"] = cBattery.MinDischargeTemperature;
    root["max_discharge_temp"] = cBattery.MaxDischargeTemperature;
    root["stop_charging_soc"] = cBattery.Stop_Charging_BatterySoC_Threshold;
    root["verbose_logging"] = Battery._verboseLogging;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiBatteryClass::onAdminGet(AsyncWebServerRequest* request)
{
    onStatus(request);
}

void WebApiBatteryClass::onAdminPost(AsyncWebServerRequest* request)
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
            && root.containsKey("provider")
            && root.containsKey("min_charge_temp")
            && root.containsKey("max_charge_temp")
            && root.containsKey("min_discharge_temp")
            && root.containsKey("max_discharge_temp")
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

    Battery_CONFIG_T& cBattery = Configuration.get().Battery;
    cBattery.Enabled = root["enabled"].as<bool>();
    cBattery.UpdatesOnly = root["updatesonly"].as<bool>();
    cBattery.Provider = root["provider"].as<uint8_t>();
    Configuration.get().MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
#ifdef USE_JKBMS_CONTROLLER
    cBattery.JkBms.Interface = root["jkbms_interface"].as<uint8_t>();
    cBattery.JkBms.PollingInterval = root["pollinterval"].as<uint32_t>(); // root["jkbms_polling_interval"].as<uint8_t>();
#endif
    cBattery.PollInterval = root["pollinterval"].as<uint32_t>();
    cBattery.MinChargeTemperature = root["min_charge_temp"].as<int8_t>();
    cBattery.MaxChargeTemperature = root["max_charge_temp"].as<int8_t>();
    cBattery.MinDischargeTemperature = root["min_discharge_temp"].as<int8_t>();
    cBattery.MaxDischargeTemperature = root["max_discharge_temp"].as<int8_t>();
    cBattery.numberOfBatteries = root["numberOfBatteries"].as<int8_t>();
    if (Configuration.get().Battery.numberOfBatteries > MAX_BATTERIES)
        Configuration.get().Battery.numberOfBatteries = MAX_BATTERIES;

#ifdef USE_MQTT_BATTERY
    strlcpy(cBattery.Mqtt.SocTopic, root["mqtt_soc_topic"].as<String>().c_str(), sizeof(cBattery.Mqtt.SocTopic));
    strlcpy(cBattery.Mqtt.VoltageTopic, root["mqtt_voltage_topic"].as<String>().c_str(), sizeof(cBattery.Mqtt.VoltageTopic));
#endif
    cBattery.Stop_Charging_BatterySoC_Threshold = root["stop_charging_soc"].as<uint8_t>();

    Battery._verboseLogging = root["verbose_logging"].as<bool>();

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    Battery.updateSettings();
#ifdef USE_HASS
    MqttHandleBatteryHass.forceUpdate();

    // potentially make SoC thresholds auto-discoverable
    MqttHandlePowerLimiterHass.forceUpdate();
#endif
}
