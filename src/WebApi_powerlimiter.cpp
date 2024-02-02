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

    _server = &server;

    _server->on("/api/powerlimiter/status", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onStatus, this, _1));
    _server->on("/api/powerlimiter/config", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onAdminGet, this, _1));
    _server->on("/api/powerlimiter/config", HTTP_POST, std::bind(&WebApiPowerLimiterClass::onAdminPost, this, _1));
}

void WebApiPowerLimiterClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;

    root["enabled"] = cPL.Enabled;
    root["pollinterval"] = cPL.PollInterval;
    root["updatesonly"] = cPL.UpdatesOnly;
    root["verbose_logging"] = PowerLimiter.getVerboseLogging();
    root["solar_passthrough_enabled"] = cPL.SolarPassThroughEnabled;
    root["solar_passthrough_losses"] = cPL.SolarPassThroughLosses;
    root["battery_drain_strategy"] = cPL.BatteryDrainStategy;
    root["is_inverter_behind_powermeter"] = cPL.IsInverterBehindPowerMeter;
    root["inverter_id"] = cPL.InverterId;
    root["inverter_channel_id"] = cPL.InverterChannelId;
    root["target_power_consumption"] = cPL.TargetPowerConsumption;
    root["target_power_consumption_hysteresis"] = cPL.TargetPowerConsumptionHysteresis;
    root["lower_power_limit"] = cPL.LowerPowerLimit;
    root["upper_power_limit"] = cPL.UpperPowerLimit;
    root["battery_soc_start_threshold"] = cPL.BatterySocStartThreshold;
    root["battery_soc_stop_threshold"] = cPL.BatterySocStopThreshold;
    root["voltage_start_threshold"] = static_cast<int>(cPL.VoltageStartThreshold * 100 + 0.5) / 100.0;
    root["voltage_stop_threshold"] = static_cast<int>(cPL.VoltageStopThreshold * 100 + 0.5) / 100.0;
    ;
    root["voltage_load_correction_factor"] = cPL.VoltageLoadCorrectionFactor;
    root["inverter_restart_hour"] = cPL.RestartHour;
    root["full_solar_passthrough_soc"] = cPL.FullSolarPassThroughSoc;
    root["full_solar_passthrough_start_voltage"] = static_cast<int>(cPL.FullSolarPassThroughStartVoltage * 100 + 0.5) / 100.0;
    root["full_solar_passthrough_stop_voltage"] = static_cast<int>(cPL.FullSolarPassThroughStopVoltage * 100 + 0.5) / 100.0;

    response->setLength();
    request->send(response);
}

void WebApiPowerLimiterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

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
            && root.containsKey("inverter_channel_id")
            && root.containsKey("target_power_consumption")
            && root.containsKey("target_power_consumption_hysteresis"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    PowerLimiter_CONFIG_T& cPL = Configuration.get().PowerLimiter;
    cPL.Enabled = root["enabled"].as<bool>();
    cPL.PollInterval = root["pollinterval"].as<uint16_t>();
    cPL.UpdatesOnly = root["updatesonly"].as<bool>();
    PowerLimiter.setVerboseLogging(root["verbose_logging"].as<bool>());
    PowerLimiter.setMode(PowerLimiterClass::Mode::Normal); // User input sets PL to normal operation
    cPL.SolarPassThroughEnabled = root["solar_passthrough_enabled"].as<bool>();
    cPL.SolarPassThroughLosses = root["solar_passthrough_losses"].as<uint8_t>();
    cPL.BatteryDrainStategy = root["battery_drain_strategy"].as<uint8_t>();
    cPL.IsInverterBehindPowerMeter = root["is_inverter_behind_powermeter"].as<bool>();
    cPL.InverterId = root["inverter_id"].as<uint8_t>();
    cPL.InverterChannelId = root["inverter_channel_id"].as<uint8_t>();
    cPL.TargetPowerConsumption = root["target_power_consumption"].as<int32_t>();
    cPL.TargetPowerConsumptionHysteresis = root["target_power_consumption_hysteresis"].as<int32_t>();
    cPL.LowerPowerLimit = root["lower_power_limit"].as<int32_t>();
    cPL.UpperPowerLimit = root["upper_power_limit"].as<int32_t>();
    cPL.BatterySocStartThreshold = root["battery_soc_start_threshold"].as<uint32_t>();
    cPL.BatterySocStopThreshold = root["battery_soc_stop_threshold"].as<uint32_t>();
    cPL.VoltageStartThreshold = static_cast<int>(root["voltage_start_threshold"].as<float>() * 100) / 100.0;
    cPL.VoltageStopThreshold = static_cast<int>(root["voltage_stop_threshold"].as<float>() * 100) / 100.0;
    cPL.VoltageLoadCorrectionFactor = root["voltage_load_correction_factor"].as<float>();
    cPL.RestartHour = root["inverter_restart_hour"].as<int8_t>();
    cPL.FullSolarPassThroughSoc = root["full_solar_passthrough_soc"].as<uint32_t>();
    cPL.FullSolarPassThroughStartVoltage = static_cast<int>(root["full_solar_passthrough_start_voltage"].as<float>() * 100) / 100.0;
    cPL.FullSolarPassThroughStopVoltage = static_cast<int>(root["full_solar_passthrough_stop_voltage"].as<float>() * 100) / 100.0;

    WebApi.writeConfig(retMsg);

    PowerLimiter.calcNextInverterRestart();

    response->setLength();
    request->send(response);
}
