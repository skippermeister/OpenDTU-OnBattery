// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_powerlimiter.h"
#include "VeDirectFrameHandler.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MqttHandlePowerLimiterHass.h"
#include "MqttHandleVedirectHass.h"
#include "PowerLimiter.h"
#include "WebApi.h"
#include "helper.h"
#include "WebApi_errors.h"

void WebApiPowerLimiterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/powerlimiter/status", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onStatus, this, _1));
    server.on("/api/powerlimiter/config", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onAdminGet, this, _1));
    server.on("/api/powerlimiter/config", HTTP_POST, std::bind(&WebApiPowerLimiterClass::onAdminPost, this, _1));
    server.on("/api/powerlimiter/metadata", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onMetaData, this, _1));
}

void WebApiPowerLimiterClass::onStatus(AsyncWebServerRequest* request)
{
    auto const& config = Configuration.get();

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["enabled"] = config.PowerLimiter.Enabled;
    root["updatesonly"] = config.PowerLimiter.UpdatesOnly;
    root["verbose_logging"] = config.PowerLimiter.VerboseLogging;
    root["solar_passthrough_enabled"] = config.PowerLimiter.SolarPassThroughEnabled;
    root["solar_passthrough_losses"] = config.PowerLimiter.SolarPassThroughLosses;
    root["battery_always_use_at_night"] = config.PowerLimiter.BatteryAlwaysUseAtNight;
    root["is_inverter_behind_powermeter"] = config.PowerLimiter.IsInverterBehindPowerMeter;
    root["is_inverter_solar_powered"] = config.PowerLimiter.IsInverterSolarPowered;
    root["use_overscaling_to_compensate_shading"] = config.PowerLimiter.UseOverscalingToCompensateShading;
    root["inverter_serial"] = String(config.PowerLimiter.InverterId); //config.Inverter[config.PowerLimiter.InverterId].Serial;
    root["inverter_channel_id"] = config.PowerLimiter.InverterChannelId;
    root["target_power_consumption"] = config.PowerLimiter.TargetPowerConsumption;
    root["target_power_consumption_hysteresis"] = config.PowerLimiter.TargetPowerConsumptionHysteresis;
    root["lower_power_limit"] = config.PowerLimiter.LowerPowerLimit;
    root["base_load_limit"] = config.PowerLimiter.BaseLoadLimit;
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
#ifdef USE_SURPLUSPOWER
    root["surplus_power_enabled"] = config.PowerLimiter.SurplusPowerEnabled;
#endif

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiPowerLimiterClass::onMetaData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    auto const& config = Configuration.get();

    size_t invAmount = 0;
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial != 0) { ++invAmount; }
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["power_meter_enabled"] = config.PowerMeter.Enabled;
    root["battery_enabled"] = config.Battery.Enabled;
    root["charge_controller_enabled"] = config.Vedirect.Enabled;
#ifdef USE_CHARGER_MEANWELL
    root["charger_enabled"] = config.MeanWell.Enabled;
#endif
#ifdef USE_CHARGER_HUAWEI
    root["charger_enabled"] = config.Huawei.Enabled;
#endif

    auto inverters = root["inverters"].to<JsonObject>();
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == 0) { continue; }

        // we use the integer (base 10) representation of the inverter serial,
        // rather than the hex represenation as used when handling the inverter
        // serial elsewhere in the web application, because in this case, the
        // serial is actually not displayed but only used as a value/index.
        auto obj = inverters[String(config.Inverter[i].Serial)].to<JsonObject>();
        obj["pos"] = i;
        obj["name"] = String(config.Inverter[i].Name);
        obj["poll_enable_day"] = config.Inverter[i].Poll_Enable_Day;
        obj["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        obj["command_enable_day"] = config.Inverter[i].Command_Enable_Day;
        obj["command_enable_night"] = config.Inverter[i].Command_Enable_Night;

        obj["type"] = "Unknown";
        obj["channels"] = 1;
        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);
        if (inv != nullptr) {
            obj["type"] = inv->typeName();
            auto channels = inv->Statistics()->getChannelsByType(TYPE_DC);
            obj["channels"] = channels.size();
        }
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
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
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    // we were not actually checking for all the keys we (unconditionally)
    // access below for a long time, and it is technically not needed if users
    // use the web application to submit settings. the web app will always
    // submit all keys. users who send HTTP requests manually need to beware
    // anyways to always include the keys accessed below. if we wanted to
    // support a simpler API, like only sending the "enabled" key which only
    // changes that key, we need to refactor all of the code below.
    if (!root.containsKey("enabled")) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto& config = Configuration.get();
    config.PowerLimiter.Enabled = root["enabled"].as<bool>();
    config.PowerLimiter.VerboseLogging = root["verbose_logging"].as<bool>();
    config.PowerLimiter.UpdatesOnly = root["updatesonly"].as<bool>();
    PowerLimiter.setMode(PowerLimiterClass::Mode::Normal); // User input sets PL to normal operation

    if (config.Vedirect.Enabled) {
        config.PowerLimiter.SolarPassThroughEnabled = root["solar_passthrough_enabled"].as<bool>();
        config.PowerLimiter.SolarPassThroughLosses = root["solar_passthrough_losses"].as<uint8_t>();
        config.PowerLimiter.FullSolarPassThroughStartVoltage = static_cast<int>(root["full_solar_passthrough_start_voltage"].as<float>() * 100) / 100.0;
        config.PowerLimiter.FullSolarPassThroughStopVoltage = static_cast<int>(root["full_solar_passthrough_stop_voltage"].as<float>() * 100) / 100.0;
    }

    config.PowerLimiter.IsInverterBehindPowerMeter = root["is_inverter_behind_powermeter"].as<bool>();
    config.PowerLimiter.IsInverterSolarPowered = root["is_inverter_solar_powered"].as<bool>();
    config.PowerLimiter.BatteryAlwaysUseAtNight= root["battery_always_use_at_night"].as<bool>();
    config.PowerLimiter.UseOverscalingToCompensateShading = root["use_overscaling_to_compensate_shading"].as<bool>();
    //config.PowerLimiter.InverterId = root["inverter_id"].as<uint64_t>();
    config.PowerLimiter.InverterId = root["inverter_serial"].as<uint64_t>();
    config.PowerLimiter.InverterChannelId = root["inverter_channel_id"].as<uint8_t>();
    config.PowerLimiter.TargetPowerConsumption = root["target_power_consumption"].as<int32_t>();
    config.PowerLimiter.TargetPowerConsumptionHysteresis = root["target_power_consumption_hysteresis"].as<int32_t>();
    config.PowerLimiter.LowerPowerLimit = root["lower_power_limit"].as<int32_t>();
    config.PowerLimiter.BaseLoadLimit = root["base_load_limit"].as<int32_t>();
    config.PowerLimiter.UpperPowerLimit = root["upper_power_limit"].as<int32_t>();
#ifdef USE_SURPLUSPOWER
    config.PowerLimiter.SurplusPowerEnabled = root["surplus_power_enabled"];
#endif

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

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    PowerLimiter.calcNextInverterRestart();

#ifdef USE_HASS
    // potentially make thresholds auto-discoverable
    MqttHandlePowerLimiterHass.forceUpdate();
#endif
}
