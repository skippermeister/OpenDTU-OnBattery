// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_powerlimiter.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "ErrorMessages.h"
#include "MqttHandleHass.h"
#include "MqttHandleVedirectHass.h"
#include "MqttSettings.h"
#include "PowerLimiter.h"
#include "PowerMeter.h"
#include "VeDirectFrameHandler.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"

void WebApiPowerLimiterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/powerlimiter/status", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onStatus, this, _1));
    server.on("/api/powerlimiter/config", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onAdminGet, this, _1));
    server.on("/api/powerlimiter/config", HTTP_POST, std::bind(&WebApiPowerLimiterClass::onAdminPost, this, _1));
}

void WebApiPowerLimiterClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root["enabled"] = config.PowerLimiter.Enabled;
    root["pollinterval"] = config.PowerLimiter.PollInterval;
    root["updatesonly"] = config.PowerLimiter.UpdatesOnly;
    root["verbose_logging"] = PowerLimiter.getVerboseLogging();
    root["solar_passthrough_enabled"] = config.PowerLimiter.SolarPassThroughEnabled;
    root["solar_passthrough_losses"] = config.PowerLimiter.SolarPassThroughLosses;
    root["battery_drain_strategy"] = config.PowerLimiter.BatteryDrainStategy;
    root["is_inverter_behind_powermeter"] = config.PowerLimiter.IsInverterBehindPowerMeter;
    root["inverter_id"] = config.PowerLimiter.InverterId;
    root["inverter_serial"] = config.Inverter[config.PowerLimiter.InverterId].Serial;
    root["inverter_channel_id"] = config.PowerLimiter.InverterChannelId;
    root["target_power_consumption"] = config.PowerLimiter.TargetPowerConsumption;
    root["target_power_consumption_hysteresis"] = config.PowerLimiter.TargetPowerConsumptionHysteresis;
    root["lower_power_limit"] = config.PowerLimiter.LowerPowerLimit;
    root["upper_power_limit"] = config.PowerLimiter.UpperPowerLimit;
    root["ignore_soc"] = config.PowerLimiter.IgnoreSoc;
    root["battery_soc_start_threshold"] = config.PowerLimiter.BatterySocStartThreshold;
    root["battery_soc_stop_threshold"] = config.PowerLimiter.BatterySocStopThreshold;
    root["voltage_start_threshold"] = static_cast<int>(config.PowerLimiter.VoltageStartThreshold * 100 + 0.5) / 100.0;
    root["voltage_stop_threshold"] = static_cast<int>(config.PowerLimiter.VoltageStopThreshold * 100 + 0.5) / 100.0;
    root["voltage_load_correction_factor"] = config.PowerLimiter.VoltageLoadCorrectionFactor;

    root["inverter_restart_hour"] = config.PowerLimiter.RestartHour;
    root["full_solar_passthrough_soc"] = config.PowerLimiter.FullSolarPassThroughSoc;
    root["full_solar_passthrough_start_voltage"] = static_cast<int>(config.PowerLimiter.FullSolarPassThroughStartVoltage * 100 + 0.5) / 100.0;
    root["full_solar_passthrough_stop_voltage"] = static_cast<int>(config.PowerLimiter.FullSolarPassThroughStopVoltage * 100 + 0.5) / 100.0;

    root["power_meter_enabled"] = config.PowerMeter.Enabled;
    root["battery_enabled"] = config.Battery.Enabled;
    root["solar_charge_controller_enabled"] = config.Vedirect.Enabled;
    root["charge_controller_enabled"] = config.MeanWell.Enabled;

    JsonArray inverters = root.createNestedArray("inverters");
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == 0) { continue; }

        JsonObject obj = inverters.createNestedObject();
        obj["id"] = i;
        obj["name"] = String(config.Inverter[i].Name);
        obj["serial"] = config.Inverter[i].Serial;
        obj["poll_enable_night"] = config.Inverter[i].Poll_Enable_Day;
        obj["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        obj["command_enable_day"] = config.Inverter[i].Command_Enable_Day;
        obj["command_enable_night"] = config.Inverter[i].Command_Enable_Night;

        obj["type"] = "Unknown";
        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);
        if (inv != nullptr) { obj["type"] = inv->typeName(); }
    }

    response->setLength();
    request->send(response);
}

void WebApiPowerLimiterClass::onAdminGet(AsyncWebServerRequest* request)
{
    this->onStatus(request);
}

void WebApiPowerLimiterClass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& retMsg = response->getRoot();
    retMsg["type"] = Warning;

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > 3 * 1024) {
        retMsg["message"] = DataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(3 * 1024);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled")
            && root.containsKey("updatesonly")
            && root.containsKey("pollinterval")
            && root.containsKey("verbose_logging")
            && root.containsKey("lower_power_limit")
            && root.containsKey("inverter_id")
            && root["inverter_id"].as<String>() != "invalid"
            && root.containsKey("inverter_channel_id")
            && root.containsKey("target_power_consumption")
            && root.containsKey("target_power_consumption_hysteresis"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    CONFIG_T& config = Configuration.get();
    config.PowerLimiter.Enabled = root["enabled"].as<bool>();
    config.PowerLimiter.PollInterval = root["pollinterval"].as<uint16_t>();
    config.PowerLimiter.UpdatesOnly = root["updatesonly"].as<bool>();
    PowerLimiter.setVerboseLogging(root["verbose_logging"].as<bool>());
    PowerLimiter.setMode(PowerLimiterClass::Mode::Normal); // User input sets PL to normal operation

    if (config.Vedirect.Enabled) {
        config.PowerLimiter.SolarPassThroughEnabled = root["solar_passthrough_enabled"].as<bool>();
        config.PowerLimiter.SolarPassThroughLosses = root["solar_passthrough_losses"].as<uint8_t>();
        config.PowerLimiter.BatteryDrainStategy = root["battery_drain_strategy"].as<uint8_t>();
        config.PowerLimiter.FullSolarPassThroughStartVoltage = static_cast<int>(root["full_solar_passthrough_start_voltage"].as<float>() * 100) / 100.0;
        config.PowerLimiter.FullSolarPassThroughStopVoltage = static_cast<int>(root["full_solar_passthrough_stop_voltage"].as<float>() * 100) / 100.0;
    }

    config.PowerLimiter.IsInverterBehindPowerMeter = root["is_inverter_behind_powermeter"].as<bool>();
    config.PowerLimiter.InverterId = root["inverter_id"].as<uint8_t>();
    config.PowerLimiter.InverterChannelId = root["inverter_channel_id"].as<uint8_t>();
    config.PowerLimiter.TargetPowerConsumption = root["target_power_consumption"].as<int32_t>();
    config.PowerLimiter.TargetPowerConsumptionHysteresis = root["target_power_consumption_hysteresis"].as<int32_t>();
    config.PowerLimiter.LowerPowerLimit = root["lower_power_limit"].as<int32_t>();
    config.PowerLimiter.UpperPowerLimit = root["upper_power_limit"].as<int32_t>();

    if (config.Battery.Enabled) {
        config.PowerLimiter.IgnoreSoc = root["ignore_soc"].as<bool>();
        config.PowerLimiter.BatterySocStartThreshold = root["battery_soc_start_threshold"].as<uint32_t>();
        config.PowerLimiter.BatterySocStopThreshold = root["battery_soc_stop_threshold"].as<uint32_t>();
        if (config.Vedirect.Enabled) {
            config.PowerLimiter.FullSolarPassThroughSoc = root["full_solar_passthrough_soc"].as<uint32_t>();
        }
    }

    config.PowerLimiter.VoltageStartThreshold = static_cast<int>(root["voltage_start_threshold"].as<float>() * 100) / 100.0;
    config.PowerLimiter.VoltageStopThreshold = static_cast<int>(root["voltage_stop_threshold"].as<float>() * 100) / 100.0;
    config.PowerLimiter.VoltageLoadCorrectionFactor = root["voltage_load_correction_factor"].as<float>();
    config.PowerLimiter.RestartHour = root["inverter_restart_hour"].as<int8_t>();

    WebApi.writeConfig(retMsg);

    PowerLimiter.calcNextInverterRestart();

    response->setLength();
    request->send(response);
}
