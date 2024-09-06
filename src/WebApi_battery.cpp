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
#include "MqttHandlePowerLimiterHass.h"
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
    auto const& config = Configuration.get();

    root["enabled"] = config.Battery.Enabled;
    root["verbose_logging"] = config.Battery.VerboseLogging;
    root["updatesonly"] = config.Battery.UpdatesOnly;
    root["provider"] = config.Battery.Provider;
    root["io_providername"] = PinMapping.get().battery.providerName;
    root["can_controller_frequency"] = config.MCP2515.Controller_Frequency;
    root["pollinterval"] = config.Battery.PollInterval;
    root["min_charge_temp"] = config.Battery.MinChargeTemperature;
    root["max_charge_temp"] = config.Battery.MaxChargeTemperature;
    root["min_discharge_temp"] = config.Battery.MinDischargeTemperature;
    root["max_discharge_temp"] = config.Battery.MaxDischargeTemperature;
    root["stop_charging_soc"] = config.Battery.Stop_Charging_BatterySoC_Threshold;
    root["numberOfBatteries"] = config.Battery.numberOfBatteries;

    JsonObject mqtt = root["mqtt"].to<JsonObject>();
#ifdef USE_MQTT_BATTERY
    mqtt["soc_topic"] = config.Battery.Mqtt.SocTopic;
    mqtt["soc_json_path"] = config.Battery.Mqtt.SocJsonPath;
    mqtt["voltage_topic"] = config.Battery.Mqtt.VoltageTopic;
    mqtt["voltage_json_path"] = config.Battery.Mqtt.VoltageJsonPath;
    mqtt["voltage_unit"] = config.Battery.Mqtt.VoltageUnit;
#else
    mqtt["soc_topic"] = "";
    mqtt["soc_json_path"] = "";
    mqtt["voltage_topic"] = "";
    mqtt["voltage_json_path"] = "";
    mqtt["voltage_unit"] = 0;
#endif
#if defined(USE_MQTT_BATTERY) || defined(USE_VICTRON_SMART_SHUNT)
    root["recommended_charge_voltage"] = config.Battery.RecommendedChargeVoltage;
    root["recommended_discharge_voltage"] = config.Battery.RecommendedDischargeVoltage;
#else
    root["recommended_charge_voltage"] = Battery.getStats()->getRecommendedChargeVoltageLimit();
    root["recommended_discharge_voltage"] = Battery.getStats()->getRecommendedDischargeVoltageLimit();;
#endif

#ifdef USE_MQTT_ZENDURE_BATTERY
    JsonObject zendure = root["zendure"].to<JsonObject>();
    zendure["device_type"] = config.Battery.Zendure.DeviceType;
    zendure["device_serial"] = config.Battery.Zendure.DeviceSerial;
    zendure["soc_min"] = config.Battery.Zendure.MinSoC;
    zendure["soc_max"] = config.Battery.Zendure.MaxSoC;
    zendure["bypass_mode"] = config.Battery.Zendure.BypassMode;
    zendure["max_output"] = config.Battery.Zendure.MaxOutput;
#endif

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiBatteryClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

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

    if (!root.containsKey("enabled") ||
        !root.containsKey("pollinterval") ||
        !root.containsKey("updatesonly") ||
        !root.containsKey("provider") ||
        !root.containsKey("verbose_logging"))
    {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["pollinterval"].as<uint32_t>() < 1 || root["pollinterval"].as<uint32_t>() > 100) {
        retMsg["message"] = "Poll interval must be a number between 1 and 100!";
        retMsg["code"] = WebApiError::MqttPublishInterval;
        retMsg["param"]["min"] = 1;
        retMsg["param"]["max"] = 100;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.Battery.Enabled = root["enabled"].as<bool>();
    config.Battery.VerboseLogging = root["verbose_logging"].as<bool>();
    config.Battery.UpdatesOnly = root["updatesonly"].as<bool>();
    config.Battery.Provider = root["provider"].as<uint8_t>();
    config.MCP2515.Controller_Frequency = root["can_controller_frequency"].as<uint32_t>();
    config.Battery.PollInterval = root["pollinterval"].as<uint32_t>();
    config.Battery.MinChargeTemperature = root["min_charge_temp"].as<int8_t>();
    config.Battery.MaxChargeTemperature = root["max_charge_temp"].as<int8_t>();
    config.Battery.MinDischargeTemperature = root["min_discharge_temp"].as<int8_t>();
    config.Battery.MaxDischargeTemperature = root["max_discharge_temp"].as<int8_t>();
    config.Battery.Stop_Charging_BatterySoC_Threshold = root["stop_charging_soc"].as<uint8_t>();
    config.Battery.numberOfBatteries = root["numberOfBatteries"].as<int8_t>();
    if (config.Battery.numberOfBatteries > MAX_BATTERIES)
        config.Battery.numberOfBatteries = MAX_BATTERIES;

#ifdef USE_MQTT_BATTERY
    strlcpy(config.Battery.Mqtt.SocTopic, root["mqtt"]["soc_topic"].as<String>().c_str(), sizeof(config.Battery.Mqtt.SocTopic));
    strlcpy(config.Battery.Mqtt.SocJsonPath, root["mqtt"]["soc_json_path"].as<String>().c_str(), sizeof(config.Battery.Mqtt.SocJsonPath));
    strlcpy(config.Battery.Mqtt.VoltageTopic, root["mqtt"]["voltage_topic"].as<String>().c_str(), sizeof(config.Battery.Mqtt.VoltageTopic));
    strlcpy(config.Battery.Mqtt.VoltageJsonPath, root["mqtt"]["voltage_json_path"].as<String>().c_str(), sizeof(config.Battery.Mqtt.VoltageJsonPath));
    config.Battery.Mqtt.VoltageUnit = static_cast<BatteryVoltageUnit>(root["mqtt"]["voltage_unit"].as<uint8_t>());
#endif
#if defined(USE_MQTT_BATTERY) || defined(USE_VICTRON_SMART_SHUNT)
    config.Battery.RecommendedChargeVoltage = root["recommended_charge_voltage"].as<float>();
    config.Battery.RecommendedDischargeVoltage = root["recommended_discharge_voltage"].as<float>();
#endif

#ifdef USE_MQTT_ZENDURE_BATTERY
    config.Battery.Zendure.DeviceType = root["zendure"]["device_type"].as<uint8_t>();
    strlcpy(config.Battery.Zendure.DeviceSerial, root["zendure"]["device_serial"].as<String>().c_str(), sizeof(config.Battery.Zendure.DeviceSerial));
    config.Battery.Zendure.MinSoC = root["zendure"]["soc_min"].as<uint8_t>();
    config.Battery.Zendure.MaxSoC = root["zendure"]["soc_max"].as<uint8_t>();
    config.Battery.Zendure.BypassMode = root["zendure"]["bypass_mode"].as<uint8_t>();
    config.Battery.Zendure.MaxOutput = root["zendure"]["max_output"].as<uint16_t>();
#endif

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    Battery.updateSettings();
#ifdef USE_HASS
    MqttHandleBatteryHass.forceUpdate();

    // potentially make SoC thresholds auto-discoverable
    MqttHandlePowerLimiterHass.forceUpdate();
#endif
}
