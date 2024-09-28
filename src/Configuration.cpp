// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "Configuration.h"
#include "MessageOutput.h"
#include "NetworkSettings.h"
#include "Utils.h"
#include "defaults.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <nvs_flash.h>

CONFIG_T config;

static constexpr char TAG[] = "[Configuration]";

void ConfigurationClass::init()
{
    memset(&config, 0x0, sizeof(config));
}

void ConfigurationClass::serializeHttpRequestConfig(HttpRequestConfig const& source, JsonObject& target)
{
    JsonObject target_http_config = target["http_request"].to<JsonObject>();
    target_http_config["url"] = source.Url;
    target_http_config["auth_type"] = source.AuthType;
    target_http_config["username"] = source.Username;
    target_http_config["password"] = source.Password;
    target_http_config["header_key"] = source.HeaderKey;
    target_http_config["header_value"] = source.HeaderValue;
    target_http_config["timeout"] = source.Timeout;
}

void ConfigurationClass::serializePowerMeterMqttConfig(PowerMeterMqttConfig const& source, JsonObject& target)
{
    JsonArray values = target["values"].to<JsonArray>();
    for (size_t i = 0; i < POWERMETER_MQTT_MAX_VALUES; ++i) {
        JsonObject t = values.add<JsonObject>();
        PowerMeterMqttValue const& s = source.Values[i];

        t["topic"] = s.Topic;
        t["json_path"] = s.JsonPath;
        t["unit"] = s.PowerUnit;
        t["sign_inverted"] = s.SignInverted;
    }
}

void ConfigurationClass::serializePowerMeterSerialSdmConfig(PowerMeterSerialSdmConfig const& source, JsonObject& target)
{
    target["baudrate"] = source.Baudrate;
    target["address"] = source.Address;
    target["polling_interval"] = source.PollingInterval;
}

void ConfigurationClass::serializePowerMeterHttpJsonConfig(PowerMeterHttpJsonConfig const& source, JsonObject& target)
{
    target["polling_interval"] = source.PollingInterval;
    target["individual_requests"] = source.IndividualRequests;

    JsonArray values = target["values"].to<JsonArray>();
    for (size_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; ++i) {
        JsonObject t = values.add<JsonObject>();
        PowerMeterHttpJsonValue const& s = source.Values[i];

        serializeHttpRequestConfig(s.HttpRequest, t);

        t["enabled"] = s.Enabled;
        t["json_path"] = s.JsonPath;
        t["unit"] = s.PowerUnit;
        t["sign_inverted"] = s.SignInverted;
    }
}

void ConfigurationClass::serializePowerMeterHttpSmlConfig(PowerMeterHttpSmlConfig const& source, JsonObject& target)
{
    target["polling_interval"] = source.PollingInterval;
    serializeHttpRequestConfig(source.HttpRequest, target);
}

bool ConfigurationClass::write()
{
    File f = LittleFS.open(CONFIG_FILENAME, "w");
    if (!f) {
        return false;
    }
    config.Cfg.SaveCount++;

    JsonDocument doc;

    JsonObject cfg = doc["cfg"].to<JsonObject>();
    cfg["version"] = config.Cfg.Version;
    cfg["save_count"] = config.Cfg.SaveCount;

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = config.WiFi.Ssid;
    wifi["password"] = config.WiFi.Password;
    wifi["ip"] = IPAddress(config.WiFi.Ip).toString();
    wifi["netmask"] = IPAddress(config.WiFi.Netmask).toString();
    wifi["gateway"] = IPAddress(config.WiFi.Gateway).toString();
    wifi["dns1"] = IPAddress(config.WiFi.Dns1).toString();
    wifi["dns2"] = IPAddress(config.WiFi.Dns2).toString();
    wifi["dhcp"] = config.WiFi.Dhcp;
    wifi["hostname"] = config.WiFi.Hostname;
    wifi["aptimeout"] = config.WiFi.ApTimeout;

    JsonObject mdns = doc["mdns"].to<JsonObject>();
    mdns["enabled"] = config.Mdns.Enabled;

#ifdef USE_SYSLOG
    JsonObject syslog = doc["syslog"].to<JsonObject>();
    syslog["enabled"] = config.Syslog.Enabled;
    syslog["hostname"] = config.Syslog.Hostname;
    syslog["port"] = config.Syslog.Port;
#endif

#ifdef USE_ModbusDTU
    JsonObject modbus = doc["modbus"].to<JsonObject>();
    modbus["modbus_tcp_enabled"] = config.Modbus.modbus_tcp_enabled;
    modbus["modbus_delaystart"] = config.Modbus.modbus_delaystart;
    modbus["mfrname"] = config.Modbus.mfrname;
    modbus["modelname"] = config.Modbus.modelname;
    modbus["options"] = config.Modbus.options;
    modbus["version"] = config.Modbus.version;
    modbus["serial"] = config.Modbus.serial;
#endif

    JsonObject ntp = doc["ntp"].to<JsonObject>();
    ntp["server"] = config.Ntp.Server;
    ntp["timezone"] = config.Ntp.Timezone;
    ntp["timezone_descr"] = config.Ntp.TimezoneDescr;
    ntp["latitude"] = config.Ntp.Latitude;
    ntp["longitude"] = config.Ntp.Longitude;
    ntp["sunsettype"] = config.Ntp.SunsetType;
    ntp["sunrise"] = config.Ntp.Sunrise;
    ntp["sunset"] = config.Ntp.Sunset;

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["enabled"] = config.Mqtt.Enabled;
    mqtt["hostname"] = config.Mqtt.Hostname;
    mqtt["port"] = config.Mqtt.Port;
    mqtt["clientid"] = config.Mqtt.ClientId;
    mqtt["username"] = config.Mqtt.Username;
    mqtt["password"] = config.Mqtt.Password;
    mqtt["topic"] = config.Mqtt.Topic;
    mqtt["retain"] = config.Mqtt.Retain;
    mqtt["publish_interval"] = config.Mqtt.PublishInterval;
    mqtt["clean_session"] = config.Mqtt.CleanSession;

    JsonObject mqtt_lwt = mqtt["lwt"].to<JsonObject>();
    mqtt_lwt["topic"] = config.Mqtt.Lwt.Topic;
    mqtt_lwt["value_online"] = config.Mqtt.Lwt.Value_Online;
    mqtt_lwt["value_offline"] = config.Mqtt.Lwt.Value_Offline;
    mqtt_lwt["qos"] = config.Mqtt.Lwt.Qos;

    JsonObject mqtt_tls = mqtt["tls"].to<JsonObject>();
    mqtt_tls["enabled"] = config.Mqtt.Tls.Enabled;
    mqtt_tls["root_ca_cert"] = config.Mqtt.Tls.RootCaCert;
    mqtt_tls["certlogin"] = config.Mqtt.Tls.CertLogin;
    mqtt_tls["client_cert"] = config.Mqtt.Tls.ClientCert;
    mqtt_tls["client_key"] = config.Mqtt.Tls.ClientKey;

#ifdef USE_HASS
    JsonObject mqtt_hass = mqtt["hass"].to<JsonObject>();
    mqtt_hass["enabled"] = config.Mqtt.Hass.Enabled;
    mqtt_hass["retain"] = config.Mqtt.Hass.Retain;
    mqtt_hass["topic"] = config.Mqtt.Hass.Topic;
    mqtt_hass["individual_panels"] = config.Mqtt.Hass.IndividualPanels;
    mqtt_hass["expire"] = config.Mqtt.Hass.Expire;
#endif

    JsonObject dtu = doc["dtu"].to<JsonObject>();
    dtu["serial"] = config.Dtu.Serial;
    dtu["pollinterval"] = config.Dtu.PollInterval;
#ifdef USE_RADIO_NRF
    dtu["nrf_pa_level"] = config.Dtu.Nrf.PaLevel;
#endif
#ifdef USE_RADIO_CMT
    dtu["cmt_pa_level"] = config.Dtu.Cmt.PaLevel;
    dtu["cmt_frequency"] = config.Dtu.Cmt.Frequency;
    dtu["cmt_country_mode"] = config.Dtu.Cmt.CountryMode;
#endif
    JsonObject security = doc["security"].to<JsonObject>();
    security["password"] = config.Security.Password;
    security["allow_readonly"] = config.Security.AllowReadonly;

    JsonObject device = doc["device"].to<JsonObject>();
    device["pinmapping"] = config.Dev_PinMapping;

#ifdef USE_DISPLAY_GRAPHIC
    JsonObject display = device["display"].to<JsonObject>();
    display["powersafe"] = config.Display.PowerSafe;
    display["screensaver"] = config.Display.ScreenSaver;
    display["rotation"] = config.Display.Rotation;
    display["contrast"] = config.Display.Contrast;
    display["language"] = config.Display.Language;
    display["diagram_duration"] = config.Display.Diagram.Duration;
    display["diagram_mode"] = config.Display.Diagram.Mode;
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    JsonArray leds = device["led"].to<JsonArray>();
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        JsonObject led = leds.add<JsonObject>();
        led["brightness"] = config.Led[i].Brightness;
    }
#endif

    JsonArray inverters = doc["inverters"].to<JsonArray>();
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        JsonObject inv = inverters.add<JsonObject>();
        inv["serial"] = config.Inverter[i].Serial;
        inv["name"] = config.Inverter[i].Name;
        inv["order"] = config.Inverter[i].Order;
        inv["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        inv["poll_enable_day"] = config.Inverter[i].Poll_Enable_Day;
        inv["command_enable_night"] = config.Inverter[i].Command_Enable_Night;
        inv["command_enable_day"] = config.Inverter[i].Command_Enable_Day;
        inv["reachable_threshold"] = config.Inverter[i].ReachableThreshold;
        inv["zero_runtime"] = config.Inverter[i].ZeroRuntimeDataIfUnrechable;
        inv["zero_day"] = config.Inverter[i].ZeroYieldDayOnMidnight;
        inv["clear_eventlog"] = config.Inverter[i].ClearEventlogOnMidnight;
        inv["yieldday_correction"] = config.Inverter[i].YieldDayCorrection;

        JsonArray channel = inv["channel"].to<JsonArray>();
        for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
            JsonObject chanData = channel.add<JsonObject>();
            chanData["name"] = config.Inverter[i].channel[c].Name;
            chanData["max_power"] = config.Inverter[i].channel[c].MaxChannelPower;
            chanData["yield_total_offset"] = config.Inverter[i].channel[c].YieldTotalOffset;
        }
    }

    JsonObject vedirect = doc["vedirect"].to<JsonObject>();
    vedirect["enabled"] = config.Vedirect.Enabled;
    vedirect["updatesonly"] = config.Vedirect.UpdatesOnly;

#ifdef USE_REFUsol_INVERTER
    JsonObject refusol = doc["refusol"].to<JsonObject>();
    refusol["enabled"] = config.REFUsol.Enabled;
    refusol["updatesonly"] = config.REFUsol.UpdatesOnly;
    refusol["pollinterval"] = config.REFUsol.PollInterval;
    refusol["uss_address"] = config.REFUsol.USSaddress;
    refusol["baudrate"] = config.REFUsol.Baudrate;
    refusol["parity"] = config.REFUsol.Parity;
    refusol["verbose_logging"] = config.REFUsol.VerboseLogging;
#endif

    JsonObject powermeter = doc["powermeter"].to<JsonObject>();
    powermeter["enabled"] = config.PowerMeter.Enabled;
    powermeter["verbose_logging"] = config.PowerMeter.VerboseLogging;
    powermeter["updatesonly"] = config.PowerMeter.UpdatesOnly;
    powermeter["source"] = config.PowerMeter.Source;

    JsonObject powermeter_mqtt = powermeter["mqtt"].to<JsonObject>();
    serializePowerMeterMqttConfig(config.PowerMeter.Mqtt, powermeter_mqtt);

    JsonObject powermeter_serial_sdm = powermeter["serial_sdm"].to<JsonObject>();
    serializePowerMeterSerialSdmConfig(config.PowerMeter.SerialSdm, powermeter_serial_sdm);

    JsonObject powermeter_http_json = powermeter["http_json"].to<JsonObject>();
    serializePowerMeterHttpJsonConfig(config.PowerMeter.HttpJson, powermeter_http_json);

    JsonObject powermeter_http_sml = powermeter["http_sml"].to<JsonObject>();
    serializePowerMeterHttpSmlConfig(config.PowerMeter.HttpSml, powermeter_http_sml);

    JsonObject powerlimiter = doc["powerlimiter"].to<JsonObject>();
    serializePowerLimiterConfig(config.PowerLimiter, powerlimiter);

    JsonObject battery = doc["battery"].to<JsonObject>();
    serializeBatteryConfig(config.Battery, battery);

    JsonObject mcp2515 = doc["mcp2515"].to<JsonObject>();
    mcp2515["can_controller_frequency"] = config.MCP2515.Controller_Frequency;

#ifdef USE_CHARGER_HUAWEI
    JsonObject huawei = doc["huawei"].to<JsonObject>();
    huawei["enabled"] = config.Huawei.Enabled;
    huawei["verbose_logging"] = config.Huawei.VerboseLogging;
    huawei["auto_power_enabled"] = config.Huawei.Auto_Power_Enabled;
    huawei["auto_power_batterysoc_limits_enabled"] = config.Huawei.Auto_Power_BatterySoC_Limits_Enabled;
    huawei["voltage_limit"] = config.Huawei.Auto_Power_Voltage_Limit;
    huawei["enable_voltage_limit"] = config.Huawei.Auto_Power_Enable_Voltage_Limit;
    huawei["lower_power_limit"] = config.Huawei.Auto_Power_Lower_Power_Limit;
    huawei["upper_power_limit"] = config.Huawei.Auto_Power_Upper_Power_Limit;
    huawei["stop_batterysoc_threshold"] = config.Huawei.Auto_Power_Stop_BatterySoC_Threshold;
    huawei["target_power_consumption"] = config.Huawei.Auto_Power_Target_Power_Consumption;
#endif
#ifdef USE_CHARGER_MEANWELL
    JsonObject meanwell = doc["meanwell"].to<JsonObject>();
    meanwell["enabled"] = config.MeanWell.Enabled;
    meanwell["pollinterval"] = config.MeanWell.PollInterval;
    meanwell["min_voltage"] = config.MeanWell.MinVoltage;
    meanwell["max_voltage"] = config.MeanWell.MaxVoltage;
    meanwell["min_current"] = config.MeanWell.MinCurrent;
    meanwell["max_current"] = config.MeanWell.MaxCurrent;
    meanwell["hysteresis"] = config.MeanWell.Hysteresis;
    meanwell["mustInverterProduce"] = config.MeanWell.mustInverterProduce;
#endif

    JsonObject zeroExport = doc["zeroExport"].to<JsonObject>();
    zeroExport["enabled"] = config.ZeroExport.Enabled;
    zeroExport["updatesonly"] = config.ZeroExport.UpdatesOnly;
    zeroExport["InverterId"] = config.ZeroExport.InverterId;
    JsonArray serials = zeroExport["serials"].to<JsonArray>();
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.ZeroExport.serials[i] != 0) {
            serials[i] = config.ZeroExport.serials[i];
        }
    }
    zeroExport["PowerHysteresis"] = config.ZeroExport.PowerHysteresis;
    zeroExport["MaxGrid"] = config.ZeroExport.MaxGrid;
    zeroExport["MinimumLimit"] = config.ZeroExport.MinimumLimit;
    zeroExport["Tn"] = config.ZeroExport.Tn;

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return false;
    }

/*
    {
        String buffer;
        serializeJsonPretty(doc, buffer);
        Serial.println(buffer);
    }
*/
    // Serialize JSON to file
    if (serializeJson(doc, f) == 0) {
        MessageOutput.println("Failed to write file");
        return false;
    }

    f.close();
    return true;
}

void ConfigurationClass::deserializeHttpRequestConfig(JsonObject const& source, HttpRequestConfig& target)
{
    JsonObject source_http_config = source["http_request"];

    strlcpy(target.Url, source_http_config["url"] | "", sizeof(target.Url));
    target.AuthType = source_http_config["auth_type"] | HttpRequestConfig::Auth::None;
    strlcpy(target.Username, source_http_config["username"] | "", sizeof(target.Username));
    strlcpy(target.Password, source_http_config["password"] | "", sizeof(target.Password));
    strlcpy(target.HeaderKey, source_http_config["header_key"] | "", sizeof(target.HeaderKey));
    strlcpy(target.HeaderValue, source_http_config["header_value"] | "", sizeof(target.HeaderValue));
    target.Timeout = source_http_config["timeout"] | HTTP_REQUEST_TIMEOUT_MS;
}

void ConfigurationClass::deserializePowerMeterMqttConfig(JsonObject const& source, PowerMeterMqttConfig& target)
{
    for (size_t i = 0; i < POWERMETER_MQTT_MAX_VALUES; ++i) {
        PowerMeterMqttValue& t = target.Values[i];
        JsonObject s = source["values"][i];

        strlcpy(t.Topic, s["topic"] | "", sizeof(t.Topic));
        strlcpy(t.JsonPath, s["json_path"] | "", sizeof(t.JsonPath));
        t.PowerUnit = s["unit"] | PowerMeterMqttValue::Unit::Watts;
        t.SignInverted = s["sign_inverted"] | false;
    }
}

void ConfigurationClass::deserializePowerMeterSerialSdmConfig(JsonObject const& source, PowerMeterSerialSdmConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    target.Baudrate = source["baudrate"] | POWERMETER_SDMBAUDRATE;
    target.Address = source["address"] | POWERMETER_SDMADDRESS;
}

void ConfigurationClass::deserializePowerMeterHttpJsonConfig(JsonObject const& source, PowerMeterHttpJsonConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    target.IndividualRequests = source["individual_requests"] | false;

    JsonArray values = source["values"].as<JsonArray>();
    for (size_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; ++i) {
        PowerMeterHttpJsonValue& t = target.Values[i];
        JsonObject s = values[i];

        deserializeHttpRequestConfig(s, t.HttpRequest);

        t.Enabled = s["enabled"] | false;
        strlcpy(t.JsonPath, s["json_path"] | "", sizeof(t.JsonPath));
        t.PowerUnit = s["unit"] | PowerMeterHttpJsonValue::Unit::Watts;
        t.SignInverted = s["sign_inverted"] | false;
    }

    target.Values[0].Enabled = true;
}

void ConfigurationClass::deserializePowerMeterHttpSmlConfig(JsonObject const& source, PowerMeterHttpSmlConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    deserializeHttpRequestConfig(source, target.HttpRequest);
}

void ConfigurationClass::serializeBatteryConfig(BatteryConfig const& source, JsonObject& target)
{
    target["enabled"] = source.Enabled;
    target["verbose_logging"] = source.VerboseLogging;
    target["updatesonly"] = source.UpdatesOnly;
    target["numberOfBatteries"] = source.numberOfBatteries;
    target["pollinterval"] = source.PollInterval;
    target["provider"] = source.Provider;
#ifdef USE_MQTT_BATTERY
    JsonObject battery_mqtt = target["mqtt"].to<JsonObject>();
    battery_mqtt["soc_topic"] = source.Mqtt.SocTopic;
    battery_mqtt["soc_json_path"] = source.Mqtt.SocJsonPath;
    battery_mqtt["voltage_topic"] = source.Mqtt.VoltageTopic;
    battery_mqtt["voltage_json_path"] = source.Mqtt.VoltageJsonPath;
    battery_mqtt["voltage_unit"] = source.Mqtt.VoltageUnit;
    battery_mqtt["discharge_current_topic"] = source.Mqtt.DischargeCurrentTopic;
    battery_mqtt["discharge_current_json_path"] = source.Mqtt.DischargeCurrentJsonPath;
    battery_mqtt["amperage_unit"] = source.Mqtt.AmperageUnit;
#endif
    target["min_charge_temp"] = source.MinChargeTemperature;
    target["max_charge_temp"] = source.MaxChargeTemperature;
    target["min_discharge_temp"] = source.MinDischargeTemperature;
    target["max_discharge_temp"] = source.MaxDischargeTemperature;
    target["stop_charging_soc"] = source.Stop_Charging_BatterySoC_Threshold;
    target["enableDischargeCurrentLimit"] = source.EnableDischargeCurrentLimit;
    target["dischargeCurrentLimit"] = static_cast<int>(source.DischargeCurrentLimit*100.0+0.5) / 100.0;
    target["useBatteryReportedDischargeCurrentLimit"] = source.UseBatteryReportedDischargeLimit;
#if defined(USE_MQTT_BATTERY) || defined(USE_VICTRON_SMART_SHUNT)
    target["recommended_charge_voltage"] = source.RecommendedChargeVoltage;
    target["recommended_discharge_voltage"] = source.RecommendedDischargeVoltage;
#endif
#ifdef USE_MQTT_ZENDURE_BATTERY
    JsonObject battery_zendure = target["zendure"].to<JsonObject>();
    battery_zendure["device_type"] = source.Zendure.DeviceType;
    battery_zendure["device_serial"] = source.Zendure.DeviceSerial;
    battery_zendure["soc_min"] = source.Zendure.MinSoC;
    battery_zendure["soc_max"] = source.Zendure.MaxSoC;
    battery_zendure["bypass_mode"] = source.Zendure.BypassMode;
    battery_zendure["max_output"] = source.Zendure.MaxOutput;
#endif
}

void ConfigurationClass::deserializeBatteryConfig(JsonObject const& source, BatteryConfig& target)
{
    target.Enabled = source["enabled"] | BATTERY_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | false;
    target.UpdatesOnly = source["updatesonly"] | BATTERY_UPDATESONLY;
    target.numberOfBatteries = source["numberOfBatteries"] | 1; // at minimum 1 battery
    if (target.numberOfBatteries > MAX_BATTERIES) target.numberOfBatteries = MAX_BATTERIES;
    target.PollInterval = source["pollinterval"] | BATTERY_POLLINTERVAL;
    target.Provider = source["provider"] | BATTERY_PROVIDER;
#ifdef USE_MQTT_BATTERY
    strlcpy(target.Mqtt.SocTopic, source["mqtt"]["soc_topic"] | "", sizeof(target.Mqtt.SocTopic));
    strlcpy(target.Mqtt.SocJsonPath, source["mqtt"]["soc_json_path"] | "", sizeof(target.Mqtt.SocJsonPath));
    strlcpy(target.Mqtt.VoltageTopic, source["mqtt"]["voltage_topic"] | "", sizeof(target.Mqtt.VoltageTopic));
    strlcpy(target.Mqtt.VoltageJsonPath, source["mqtt"]["voltage_json_path"] | "", sizeof(target.Mqtt.VoltageJsonPath));
    target.Mqtt.VoltageUnit = source["mqtt"]["voltage_unit"] | BatteryVoltageUnit::Volts;
    strlcpy(target.Mqtt.DischargeCurrentTopic, source["mqtt"]["discharge_current_topic"] | "", sizeof(target.Mqtt.DischargeCurrentTopic));
    strlcpy(target.Mqtt.DischargeCurrentJsonPath, source["mqtt"]["discharge_current_json_path"] | "", sizeof(target.Mqtt.DischargeCurrentJsonPath));
    target.Mqtt.AmperageUnit = source["mqtt"]["voltage_unit"] | BatteryAmperageUnit::Amps;
#endif
    target.MinChargeTemperature = source["min_charge_temp"] | BATTERY_MIN_CHARGE_TEMPERATURE;
    target.MaxChargeTemperature = source["max_charge_temp"] | BATTERY_MAX_CHARGE_TEMPERATURE;
    target.MinDischargeTemperature = source["min_discharge_temp"] | BATTERY_MIN_DISCHARGE_TEMPERATURE;
    target.MaxDischargeTemperature = source["max_discharge_temp"] | BATTERY_MAX_DISCHARGE_TEMPERATURE;
    target.Stop_Charging_BatterySoC_Threshold = source["stop_charging_soc"] | 100;
    target.EnableDischargeCurrentLimit = source["enableDischargeCurrentLimit"] | BATTERY_ENABLE_DISCHARGE_CURRENT_LIMIT;
    target.UseBatteryReportedDischargeLimit = source["useBatteryReportedDischargeCurrentLimit"] | BATTERY_USE_BATTERY_REPORTED_DISCHARGE_CURRENT_LIMIT;
    target.DischargeCurrentLimit = source["dischargeCurrentLimit"] | BATTERY_DISCHARGE_CURRENT_LIMIT;
    target.DischargeCurrentLimit = static_cast<int>(target.DischargeCurrentLimit * 10) / 10.0;
#if defined(USE_MQTT_BATTERY) || defined(USE_VICTRON_SMART_SHUNT)
    target.RecommendedChargeVoltage = source["recommended_charge_voltage"] | 28.4;
    target.RecommendedChargeVoltage = static_cast<int>(target.RecommendedChargeVoltage * 100) / 100.0;
    target.RecommendedDischargeVoltage = source["recommended_discharge_voltage"] | 21.0;
    target.RecommendedDischargeVoltage = static_cast<int>(target.RecommendedDischargeVoltage * 100) / 100.0;
#endif
#ifdef USE_MQTT_ZENDURE_BATTERY
    target.Zendure.DeviceType = source["zendure"]["device_type"] | BATTERY_ZENDURE_DEVICE;
    strlcpy(target.Zendure.DeviceSerial, source["zendure"]["device_serial"] | "", sizeof(target.Zendure.DeviceSerial));
    target.Zendure.MinSoC = source["zendure"]["soc_min"] | BATTERY_ZENDURE_MIN_SOC;
    target.Zendure.MaxSoC = source["zendure"]["soc_max"] | BATTERY_ZENDURE_MAX_SOC;
    target.Zendure.BypassMode = source["zendure"]["bypass_mode"] | BATTERY_ZENDURE_BYPASS_MODE;
    target.Zendure.MaxOutput = source["zendure"]["max_output"] | BATTERY_ZENDURE_MAX_OUTPUT;
#endif
}

void ConfigurationClass::serializePowerLimiterConfig(PowerLimiterConfig const& source, JsonObject& target)
{
    target["enabled"] = source.Enabled;
    target["updatesonly"] = source.UpdatesOnly;
    target["verbose_logging"] = source.VerboseLogging;
    target["solar_passthrough_enabled"] = source.SolarPassThroughEnabled;
    target["solar_passthrough_losses"] = source.SolarPassThroughLosses;
    target["battery_always_use_at_night"] = source.BatteryAlwaysUseAtNight;
    target["is_inverter_behind_powermeter"] = source.IsInverterBehindPowerMeter;
    target["is_inverter_solar_powered"] = source.IsInverterSolarPowered;
    target["use_overscaling_to_compensate_shading"] = source.UseOverscalingToCompensateShading;
    target["inverter_serial"] = String(source.InverterId); //config.Inverter[config.PowerLimiter.InverterId].Serial;
    target["inverter_channel_id"] = source.InverterChannelId;
    target["target_power_consumption"] = source.TargetPowerConsumption;
    target["target_power_consumption_hysteresis"] = source.TargetPowerConsumptionHysteresis;
    target["lower_power_limit"] = source.LowerPowerLimit;
    target["base_load_limit"] = source.BaseLoadLimit;
    target["upper_power_limit"] = source.UpperPowerLimit;
    target["ignore_soc"] = source.IgnoreSoc;
    target["battery_soc_start_threshold"] = source.BatterySocStartThreshold;
    target["battery_soc_stop_threshold"] = source.BatterySocStopThreshold;
    target["voltage_start_threshold"] = static_cast<int>(source.VoltageStartThreshold * 100.0 + 0.5) / 100.0;
    target["voltage_stop_threshold"] = static_cast<int>(source.VoltageStopThreshold * 100.0 + 0.5) / 100.0;
    target["voltage_load_correction_factor"] = source.VoltageLoadCorrectionFactor;
    target["inverter_restart_hour"] = source.RestartHour;
    target["full_solar_passthrough_soc"] = source.FullSolarPassThroughSoc;
    target["full_solar_passthrough_start_voltage"] = static_cast<int>(source.FullSolarPassThroughStartVoltage * 100.0 + 0.5) / 100.0;
    target["full_solar_passthrough_stop_voltage"] = static_cast<int>(source.FullSolarPassThroughStopVoltage * 100.0 + 0.5) / 100.0;
#ifdef USE_SURPLUSPOWER
    target["surplus_power_enabled"] = source.SurplusPowerEnabled;
#endif

}

void ConfigurationClass::deserializePowerLimiterConfig(JsonObject const& source, PowerLimiterConfig& target)
{
    target.Enabled = source["enabled"] | POWERLIMITER_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | false;
    target.UpdatesOnly = source["updatesonly"] | POWERLIMITER_UPDATESONLY;
    target.SolarPassThroughEnabled = source["solar_passthrough_enabled"] | POWERLIMITER_SOLAR_PASSTHROUGH_ENABLED;
    target.SolarPassThroughLosses = source["solar_passthrough_losses"] | POWERLIMITER_SOLAR_PASSTHROUGH_LOSSES;
    target.BatteryAlwaysUseAtNight = source["battery_always_use_at_night"] | POWERLIMITER_BATTERY_ALWAYS_USE_AT_NIGHT;
    if (source["battery_drain_strategy"].as<uint8_t>() == 1) { target.BatteryAlwaysUseAtNight = true; } // convert legacy setting
    target.IsInverterBehindPowerMeter = source["is_inverter_behind_powermeter"] | POWERLIMITER_IS_INVERTER_BEHIND_POWER_METER;
    target.IsInverterSolarPowered = source["is_inverter_solar_powered"] | POWERLIMITER_IS_INVERTER_SOLAR_POWERED;
    target.UseOverscalingToCompensateShading = source["use_overscaling_to_compensate_shading"] | POWERLIMITER_USE_OVERSCALING_TO_COMPENSATE_SHADING;
    target.InverterId = source["inverter_serial"].as<uint64_t>() | POWERLIMITER_INVERTER_ID;
    if (target.InverterId == POWERLIMITER_INVERTER_ID) // legacy
        target.InverterId = source["inverter_id"].as<uint64_t>() | POWERLIMITER_INVERTER_ID;
    target.InverterChannelId = source["inverter_channel_id"].as<uint8_t>() | POWERLIMITER_INVERTER_CHANNEL_ID;
    target.TargetPowerConsumption = source["target_power_consumption"] | POWERLIMITER_TARGET_POWER_CONSUMPTION;
    target.TargetPowerConsumptionHysteresis = source["target_power_consumption_hysteresis"] | POWERLIMITER_TARGET_POWER_CONSUMPTION_HYSTERESIS;
    target.LowerPowerLimit = source["lower_power_limit"] | POWERLIMITER_LOWER_POWER_LIMIT;
    target.BaseLoadLimit = source["base_load_limit"] | POWERLIMITER_BASE_LOAD_LIMIT;
    target.UpperPowerLimit = source["upper_power_limit"] | POWERLIMITER_UPPER_POWER_LIMIT;
    target.VoltageStartThreshold = source["voltage_start_threshold"] | POWERLIMITER_VOLTAGE_START_THRESHOLD;
    target.VoltageStartThreshold = static_cast<int>(target.VoltageStartThreshold * 100.0 + 0.5) / 100.0;
    target.VoltageStopThreshold = source["voltage_stop_threshold"] | POWERLIMITER_VOLTAGE_STOP_THRESHOLD;
    target.VoltageStopThreshold = static_cast<int>(target.VoltageStopThreshold * 100.0 + 0.5) / 100.0;
    target.VoltageLoadCorrectionFactor = source["voltage_load_correction_factor"] | POWERLIMITER_VOLTAGE_LOAD_CORRECTION_FACTOR;
    target.RestartHour = source["inverter_restart_hour"].as<int8_t>() | POWERLIMITER_RESTART_HOUR;

    target.IgnoreSoc = source["ignore_soc"] | POWERLIMITER_IGNORE_SOC;
    target.BatterySocStartThreshold = source["battery_soc_start_threshold"] | POWERLIMITER_BATTERY_SOC_START_THRESHOLD;
    target.BatterySocStopThreshold = source["battery_soc_stop_threshold"] | POWERLIMITER_BATTERY_SOC_STOP_THRESHOLD;
    target.FullSolarPassThroughSoc = source["full_solar_passthrough_soc"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_SOC;
    target.FullSolarPassThroughStartVoltage = source["full_solar_passthrough_start_voltage"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_START_VOLTAGE;
    target.FullSolarPassThroughStopVoltage = source["full_solar_passthrough_stop_voltage"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_STOP_VOLTAGE;

#ifdef USE_SURPLUSPOWER
    target.SurplusPowerEnabled = source["surplus_power_enabled"] | false;
#endif
}

bool ConfigurationClass::read()
{
    File f = LittleFS.open(CONFIG_FILENAME, "r", false);

    JsonDocument doc;

    // Deserialize the JSON document
    const DeserializationError error = deserializeJson(doc, f);
    if (error) {
        MessageOutput.println("Failed to read file, using default configuration");
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return false;
    }

    JsonObject cfg = doc["cfg"];
    config.Cfg.Version = cfg["version"] | CONFIG_VERSION;
    config.Cfg.SaveCount = cfg["save_count"] | 0;

    JsonObject wifi = doc["wifi"];
    strlcpy(config.WiFi.Ssid, wifi["ssid"] | WIFI_SSID, sizeof(config.WiFi.Ssid));
    strlcpy(config.WiFi.Password, wifi["password"] | WIFI_PASSWORD, sizeof(config.WiFi.Password));
    strlcpy(config.WiFi.Hostname, wifi["hostname"] | APP_HOSTNAME, sizeof(config.WiFi.Hostname));

    IPAddress wifi_ip;
    wifi_ip.fromString(wifi["ip"] | "");
    config.WiFi.Ip[0] = wifi_ip[0];
    config.WiFi.Ip[1] = wifi_ip[1];
    config.WiFi.Ip[2] = wifi_ip[2];
    config.WiFi.Ip[3] = wifi_ip[3];

    IPAddress wifi_netmask;
    wifi_netmask.fromString(wifi["netmask"] | "");
    config.WiFi.Netmask[0] = wifi_netmask[0];
    config.WiFi.Netmask[1] = wifi_netmask[1];
    config.WiFi.Netmask[2] = wifi_netmask[2];
    config.WiFi.Netmask[3] = wifi_netmask[3];

    IPAddress wifi_gateway;
    wifi_gateway.fromString(wifi["gateway"] | "");
    config.WiFi.Gateway[0] = wifi_gateway[0];
    config.WiFi.Gateway[1] = wifi_gateway[1];
    config.WiFi.Gateway[2] = wifi_gateway[2];
    config.WiFi.Gateway[3] = wifi_gateway[3];

    IPAddress wifi_dns1;
    wifi_dns1.fromString(wifi["dns1"] | "");
    config.WiFi.Dns1[0] = wifi_dns1[0];
    config.WiFi.Dns1[1] = wifi_dns1[1];
    config.WiFi.Dns1[2] = wifi_dns1[2];
    config.WiFi.Dns1[3] = wifi_dns1[3];

    IPAddress wifi_dns2;
    wifi_dns2.fromString(wifi["dns2"] | "");
    config.WiFi.Dns2[0] = wifi_dns2[0];
    config.WiFi.Dns2[1] = wifi_dns2[1];
    config.WiFi.Dns2[2] = wifi_dns2[2];
    config.WiFi.Dns2[3] = wifi_dns2[3];

    config.WiFi.Dhcp = wifi["dhcp"] | WIFI_DHCP;
    config.WiFi.ApTimeout = wifi["aptimeout"] | ACCESS_POINT_TIMEOUT;

    JsonObject mdns = doc["mdns"];
    config.Mdns.Enabled = mdns["enabled"] | MDNS_ENABLED;

#ifdef USE_SYSLOG
    JsonObject syslog = doc["syslog"];
    config.Syslog.Enabled = syslog["enabled"] | SYSLOG_ENABLED;
    strlcpy(config.Syslog.Hostname, syslog["hostname"] | "", sizeof(config.Syslog.Hostname));
    config.Syslog.Port = syslog["port"] | SYSLOG_PORT;
#endif

#ifdef USE_ModbusDTU
    JsonObject modbus = doc["modbus"];
    config.modbus.Modbus_tcp_enabled = modbus["modbus_tcp_enabled"] | MODBUS_ENABLED;
    config.modbus.Modbus_delaystart = modbus["modbus_delaystart"] | MODBUS_DELAY_START;
    strlcpy(config.Modbus.mfrname, modbus["mfrname"] | MODBUS_MFRNAME, sizeof(config.Modbus.mfrname));
    strlcpy(config.Modbus.modelname, modbus["modelname"] | MODBUS_MODELNAME, sizeof(config.Modbus.modelname));
    strlcpy(config.Modbus.options, modbus["options"] | MODBUS_OPTIONS, sizeof(config.Modbus.options));
    strlcpy(config.Modbus.version, modbus["version"] | MODBUS_VERSION, sizeof(config.Modbus.version));
    strlcpy(config.Modbus.serial, modbus["serial"] | "", sizeof(config.Modbus.serial));
#endif

    JsonObject ntp = doc["ntp"];
    strlcpy(config.Ntp.Server, ntp["server"] | NTP_SERVER, sizeof(config.Ntp.Server));
    strlcpy(config.Ntp.Timezone, ntp["timezone"] | NTP_TIMEZONE, sizeof(config.Ntp.Timezone));
    strlcpy(config.Ntp.TimezoneDescr, ntp["timezone_descr"] | NTP_TIMEZONEDESCR, sizeof(config.Ntp.TimezoneDescr));
    config.Ntp.Latitude = ntp["latitude"] | NTP_LATITUDE;
    config.Ntp.Longitude = ntp["longitude"] | NTP_LONGITUDE;
    config.Ntp.SunsetType = ntp["sunsettype"] | NTP_SUNSETTYPE;
    config.Ntp.Sunrise = ntp["sunrise"] | 90.0;
    config.Ntp.Sunset = ntp["sunset"] | 90.0;

    JsonObject mqtt = doc["mqtt"];
    config.Mqtt.Enabled = mqtt["enabled"] | MQTT_ENABLED;
    strlcpy(config.Mqtt.Hostname, mqtt["hostname"] | MQTT_HOST, sizeof(config.Mqtt.Hostname));
    config.Mqtt.Port = mqtt["port"] | MQTT_PORT;
    strlcpy(config.Mqtt.ClientId, mqtt["clientid"] | NetworkSettings.getApName().c_str(), sizeof(config.Mqtt.ClientId));    strlcpy(config.Mqtt.Username, mqtt["username"] | MQTT_USER, sizeof(config.Mqtt.Username));
    strlcpy(config.Mqtt.Password, mqtt["password"] | MQTT_PASSWORD, sizeof(config.Mqtt.Password));
    strlcpy(config.Mqtt.Topic, mqtt["topic"] | MQTT_TOPIC, sizeof(config.Mqtt.Topic));
    config.Mqtt.Retain = mqtt["retain"] | MQTT_RETAIN;
    config.Mqtt.PublishInterval = mqtt["publish_interval"] | MQTT_PUBLISH_INTERVAL;
    config.Mqtt.CleanSession = mqtt["clean_session"] | MQTT_CLEAN_SESSION;

    JsonObject mqtt_lwt = mqtt["lwt"];
    strlcpy(config.Mqtt.Lwt.Topic, mqtt_lwt["topic"] | MQTT_LWT_TOPIC, sizeof(config.Mqtt.Lwt.Topic));
    strlcpy(config.Mqtt.Lwt.Value_Online, mqtt_lwt["value_online"] | MQTT_LWT_ONLINE, sizeof(config.Mqtt.Lwt.Value_Online));
    strlcpy(config.Mqtt.Lwt.Value_Offline, mqtt_lwt["value_offline"] | MQTT_LWT_OFFLINE, sizeof(config.Mqtt.Lwt.Value_Offline));
    config.Mqtt.Lwt.Qos = mqtt_lwt["qos"] | MQTT_LWT_QOS;

    JsonObject mqtt_tls = mqtt["tls"];
    config.Mqtt.Tls.Enabled = mqtt_tls["enabled"] | MQTT_TLS;
    strlcpy(config.Mqtt.Tls.RootCaCert, mqtt_tls["root_ca_cert"] | MQTT_ROOT_CA_CERT, sizeof(config.Mqtt.Tls.RootCaCert));
    config.Mqtt.Tls.CertLogin = mqtt_tls["certlogin"] | MQTT_TLSCERTLOGIN;
    strlcpy(config.Mqtt.Tls.ClientCert, mqtt_tls["client_cert"] | MQTT_TLSCLIENTCERT, sizeof(config.Mqtt.Tls.ClientCert));
    strlcpy(config.Mqtt.Tls.ClientKey, mqtt_tls["client_key"] | MQTT_TLSCLIENTKEY, sizeof(config.Mqtt.Tls.ClientKey));

#ifdef USE_HASS
    JsonObject mqtt_hass = mqtt["hass"];
    config.Mqtt.Hass.Enabled = mqtt_hass["enabled"] | MQTT_HASS_ENABLED;
    config.Mqtt.Hass.Retain = mqtt_hass["retain"] | MQTT_HASS_RETAIN;
    config.Mqtt.Hass.Expire = mqtt_hass["expire"] | MQTT_HASS_EXPIRE;
    config.Mqtt.Hass.IndividualPanels = mqtt_hass["individual_panels"] | MQTT_HASS_INDIVIDUALPANELS;
    strlcpy(config.Mqtt.Hass.Topic, mqtt_hass["topic"] | MQTT_HASS_TOPIC, sizeof(config.Mqtt.Hass.Topic));
#endif

    JsonObject dtu = doc["dtu"];
    config.Dtu.Serial = dtu["serial"] | DTU_SERIAL;
    config.Dtu.PollInterval = dtu["pollinterval"] | DTU_POLLINTERVAL;
#ifdef USE_RADIO_NRF
    config.Dtu.Nrf.PaLevel = dtu["nrf_pa_level"] | DTU_NRF_PA_LEVEL;
#endif
#ifdef USE_RADIO_CMT
    config.Dtu.Cmt.PaLevel = dtu["cmt_pa_level"] | DTU_CMT_PA_LEVEL;
    config.Dtu.Cmt.Frequency = dtu["cmt_frequency"] | DTU_CMT_FREQUENCY;
    config.Dtu.Cmt.CountryMode = dtu["cmt_country_mode"] | DTU_CMT_COUNTRY_MODE;
#endif
    JsonObject security = doc["security"];
    strlcpy(config.Security.Password, security["password"] | ACCESS_POINT_PASSWORD, sizeof(config.Security.Password));
    config.Security.AllowReadonly = security["allow_readonly"] | SECURITY_ALLOW_READONLY;

    JsonObject device = doc["device"];
    strlcpy(config.Dev_PinMapping, device["pinmapping"] | DEV_PINMAPPING, sizeof(config.Dev_PinMapping));

#ifdef USE_DISPLAY_GRAPHIC
    JsonObject display = device["display"];
    config.Display.PowerSafe = display["powersafe"] | DISPLAY_POWERSAFE;
    config.Display.ScreenSaver = display["screensaver"] | DISPLAY_SCREENSAVER;
    config.Display.Rotation = display["rotation"] | DISPLAY_ROTATION;
    config.Display.Contrast = display["contrast"] | DISPLAY_CONTRAST;
    config.Display.Language = display["language"] | DISPLAY_LANGUAGE;
    config.Display.Diagram.Duration = display["diagram_duration"] | DISPLAY_DIAGRAM_DURATION;
    config.Display.Diagram.Mode = display["diagram_mode"] | DISPLAY_DIAGRAM_MODE;
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    JsonArray leds = device["led"];
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        JsonObject led = leds[i].as<JsonObject>();
        config.Led[i].Brightness = led["brightness"] | LED_BRIGHTNESS;
    }
#endif

    JsonArray inverters = doc["inverters"];
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        JsonObject inv = inverters[i].as<JsonObject>();
        config.Inverter[i].Serial = inv["serial"] | 0ULL;
        strlcpy(config.Inverter[i].Name, inv["name"] | "", sizeof(config.Inverter[i].Name));
        config.Inverter[i].Order = inv["order"] | 0;

        config.Inverter[i].Poll_Enable_Night = inv["poll_enable_night"] | true;
        config.Inverter[i].Poll_Enable_Day = inv["poll_enable_day"] | true;
        config.Inverter[i].Command_Enable_Night = inv["command_enable_night"] | true;
        config.Inverter[i].Command_Enable_Day = inv["command_enable_day"] | true;
        config.Inverter[i].ReachableThreshold = inv["reachable_threshold"] | REACHABLE_THRESHOLD;
        config.Inverter[i].ZeroRuntimeDataIfUnrechable = inv["zero_runtime"] | false;
        config.Inverter[i].ZeroYieldDayOnMidnight = inv["zero_day"] | false;
        config.Inverter[i].ClearEventlogOnMidnight = inv["clear_eventlog"] | false;
        config.Inverter[i].YieldDayCorrection = inv["yieldday_correction"] | false;

        JsonArray channel = inv["channel"];
        for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
            config.Inverter[i].channel[c].MaxChannelPower = channel[c]["max_power"] | 0;
            config.Inverter[i].channel[c].YieldTotalOffset = channel[c]["yield_total_offset"] | 0.0f;
            strlcpy(config.Inverter[i].channel[c].Name, channel[c]["name"] | "", sizeof(config.Inverter[i].channel[c].Name));
        }
    }

    JsonObject vedirect = doc["vedirect"];
    config.Vedirect.Enabled = vedirect["enabled"] | VEDIRECT_ENABLED;
    config.Vedirect.UpdatesOnly = vedirect["updatesonly"] | VEDIRECT_UPDATESONLY;

#ifdef USE_REFUsol_INVERTER
    JsonObject refusol = doc["refusol"];
    config.REFUsol.Enabled = refusol["enabled"] | REFUsol_ENABLED;
    config.REFUsol.UpdatesOnly = refusol["updatesonly"] | REFUsol_UPDATESONLY;
    config.REFUsol.PollInterval = refusol["pollinterval"] | REFUsol_POLLINTERVAL;
    config.REFUsol.USSaddress = refusol["uss_address"] | 1;
    config.REFUsol.Baudrate = refusol["baudrate"] | 57600;
    config.REFUsol.Parity = refusol["parity"] | REFUsol_CONFIG_T::Parity_t::None;
    config.REFUsol.VerboseLogging = refusol["verbose_logging"] | false;
#endif

    JsonObject powermeter = doc["powermeter"];
    config.PowerMeter.Enabled = powermeter["enabled"] | POWERMETER_ENABLED;
    config.PowerMeter.VerboseLogging = powermeter["verbose_logging"] | false;
    config.PowerMeter.UpdatesOnly = powermeter["updatesonly"] | POWERMETER_UPDATESONLY;
    config.PowerMeter.Source = powermeter["source"] | POWERMETER_SOURCE;

    deserializePowerMeterMqttConfig(powermeter["mqtt"], config.PowerMeter.Mqtt);

    deserializePowerMeterSerialSdmConfig(powermeter["serial_sdm"], config.PowerMeter.SerialSdm);

    JsonObject powermeter_http_json = powermeter["http_json"];
    deserializePowerMeterHttpJsonConfig(powermeter_http_json, config.PowerMeter.HttpJson);

    JsonObject powermeter_sml = powermeter["http_sml"];
    deserializePowerMeterHttpSmlConfig(powermeter_sml, config.PowerMeter.HttpSml);

    deserializePowerLimiterConfig(doc["powerlimiter"], config.PowerLimiter);

    deserializeBatteryConfig(doc["battery"], config.Battery);

    JsonObject mcp2515 = doc["mcp2515"];
    config.MCP2515.Controller_Frequency = mcp2515["can_controller_frequency"] | MCP2515_CAN_CONTROLLER_FREQUENCY;

#ifdef USE_CHARGER_HUAWEI
    JsonObject huawei = doc["huawei"];
    config.Huawei.Enabled = huawei["enabled"] | HUAWEI_ENABLED;
    config.Huawei.VerboseLogging = huawei["verbose_logging"] | false;
    config.Huawei.Auto_Power_Enabled = huawei["auto_power_enabled"] | false;
    config.Huawei.Auto_Power_BatterySoC_Limits_Enabled = huawei["auto_power_batterysoc_limits_enabled"] | false;
    config.Huawei.Emergency_Charge_Enabled = huawei["emergency_charge_enabled"] | false;
    config.Huawei.Auto_Power_Voltage_Limit = huawei["voltage_limit"] | HUAWEI_AUTO_POWER_VOLTAGE_LIMIT;
    config.Huawei.Auto_Power_Enable_Voltage_Limit = huawei["enable_voltage_limit"] | HUAWEI_AUTO_POWER_ENABLE_VOLTAGE_LIMIT;
    config.Huawei.Auto_Power_Lower_Power_Limit = huawei["lower_power_limit"] | HUAWEI_AUTO_POWER_LOWER_POWER_LIMIT;
    config.Huawei.Auto_Power_Upper_Power_Limit = huawei["upper_power_limit"] | HUAWEI_AUTO_POWER_UPPER_POWER_LIMIT;
    config.Huawei.Auto_Power_Stop_BatterySoC_Threshold = huawei["stop_batterysoc_threshold"] | HUAWEI_AUTO_POWER_STOP_BATTERYSOC_THRESHOLD;
    config.Huawei.Auto_Power_Target_Power_Consumption = huawei["target_power_consumption"] | HUAWEI_AUTO_POWER_TARGET_POWER_CONSUMPTION;
#endif
#ifdef USE_CHARGER_MEANWELL
    JsonObject meanwell = doc["meanwell"];
    config.MeanWell.Enabled = meanwell["enabled"] | MEANWELL_ENABLED;
    config.MeanWell.UpdatesOnly = meanwell["updatesonly"] | MEANWELL_UPDATESONLY;
    config.MeanWell.PollInterval = meanwell["pollinterval"] | MEANWELL_POLLINTERVAL;
    config.MeanWell.MinVoltage = meanwell["min_voltage"] | MEANWELL_MINVOLTAGE;
    config.MeanWell.MaxVoltage = meanwell["max_voltage"] | MEANWELL_MAXVOLTAGE;
    config.MeanWell.MinCurrent = meanwell["min_current"] | MEANWELL_MINCURRENT;
    config.MeanWell.MaxCurrent = meanwell["max_current"] | MEANWELL_MAXCURRENT;
    config.MeanWell.Hysteresis = meanwell["hystersis"] | MEANWELL_HYSTERESIS;
    config.MeanWell.CurrentLimitMin = 0.0;
    config.MeanWell.CurrentLimitMax = 50.0;
    config.MeanWell.VoltageLimitMin = 21.0;
    config.MeanWell.VoltageLimitMax = 80.0;
    config.MeanWell.mustInverterProduce = meanwell["mustInverterProduce"] | true;
#endif

    JsonObject zeroExport = doc["zeroExport"];
    config.ZeroExport.Enabled = zeroExport["enabled"] | ZERO_EXPORT_ENABLED;
    config.ZeroExport.UpdatesOnly = zeroExport["updatesonly"] | ZERO_EXPORT_UPDATESONLY;
    config.ZeroExport.InverterId = zeroExport["InverterId"] | ZERO_EXPORT_INVERTER_ID;
    if (zeroExport["serials"].is<JsonArray>()) {
        JsonArray serials = zeroExport["serials"].as<JsonArray>();
        if (serials.size() > 0 && serials.size() <= INV_MAX_COUNT) {
            for (uint8_t i=0; i< INV_MAX_COUNT; i++) { config.ZeroExport.serials[i] = 0; }
            uint8_t serialCount = 0;
            for (JsonVariant serial : serials) {
                config.ZeroExport.serials[serialCount] = serial.as<uint64_t>();
                MessageOutput.printf("%s%s: Serial No: %" PRIx64 "\r\n", TAG, __FUNCTION__, config.ZeroExport.serials[serialCount]);
                serialCount++;
            }
        }
    } else { // migrate from old bid mask version
        for (uint8_t i=0; i< INV_MAX_COUNT; i++) { config.ZeroExport.serials[i] = 0; }
        uint8_t serialCount = 0;
        for (uint8_t i=0; i<INV_MAX_COUNT; i++) {
            if (config.ZeroExport.InverterId & (1<<i)) {
                config.ZeroExport.serials[serialCount] = config.Inverter[i].Serial;
                MessageOutput.printf("%s%s: migrate ID%02d to Serial No: %" PRIx64 "\r\n", TAG, __FUNCTION__, i, config.ZeroExport.serials[serialCount]);
                serialCount++;
            }
        }
    }
    config.ZeroExport.PowerHysteresis = zeroExport["PowerHysteresis"] | ZERO_EXPORT_POWER_HYSTERESIS;
    config.ZeroExport.MaxGrid = zeroExport["MaxGrid"] | ZERO_EXPORT_MAX_GRID;
    config.ZeroExport.MinimumLimit = zeroExport["MinimumLimit"] | ZERO_EXPORT_MINIMUM_LIMIT;
    config.ZeroExport.Tn = zeroExport["Tn"] | ZERO_EXPORT_TN;

    f.close();
    return true;
}

void ConfigurationClass::migrate()
{
    File f = LittleFS.open(CONFIG_FILENAME, "r", false);
    if (!f) {
        MessageOutput.println("Failed to open file, cancel migration");
        return;
    }

    JsonDocument doc;

    // Deserialize the JSON document
    const DeserializationError error = deserializeJson(doc, f);
    if (error) {
        MessageOutput.printf("Failed to read file, cancel migration: %s\r\n", error.c_str());
        return;
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return;
    }

    /*
    if (config.Cfg.Version < 0x00011700) {
        JsonArray inverters = doc["inverters"];
        for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
            JsonObject inv = inverters[i].as<JsonObject>();
            JsonArray channels = inv["channels"];
            for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                config.Inverter[i].channel[c].MaxChannelPower = channels[c];
                strlcpy(config.Inverter[i].channel[c].Name, "", sizeof(config.Inverter[i].channel[c].Name));
            }
        }
    }

    if (config.Cfg.Version < 0x00011800) {
        JsonObject mqtt = doc["mqtt"];
        config.Mqtt.PublishInterval = mqtt["publish_invterval"];
    }

    if  (config.Cfg.Version < 0x00011900) {
        JsonObject dtu = doc["dtu"];
        config.Dtu.Nrf.PaLevel = dtu["pa_level"];
    }

    if (config.Cfg.Version < 0x00011a00) {
        // This migration fixes this issue: https://github.com/espressif/arduino-esp32/issues/8828
        // It occours when migrating from Core 2.0.9 to 2.0.14
        // which was done by updating ESP32 PlatformIO from 6.3.2 to 6.5.0
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (config.Cfg.Version < 0x00011c00) {
        if (!strcmp(config.Ntp.Server, NTP_SERVER_OLD)) {
            strlcpy(config.Ntp.Server, NTP_SERVER, sizeof(config.Ntp.Server));
        }
    }
    */

    f.close();

    if (config.Cfg.Version < 0x00011b00) {
        // Convert from kHz to Hz
#ifdef USE_RADIO_CMT
        config.Dtu.Cmt.Frequency *= 1000;
#endif
    }

    config.Cfg.Version = CONFIG_VERSION;
    write();
    read();
}

CONFIG_T& ConfigurationClass::get()
{
    return config;
}

INVERTER_CONFIG_T* ConfigurationClass::getFreeInverterSlot()
{
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == 0) {
            return &config.Inverter[i];
        }
    }

    return NULL;
}

INVERTER_CONFIG_T* ConfigurationClass::getInverterConfig(uint64_t serial)
{
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == serial) {
            return &config.Inverter[i];
        }
    }

    return NULL;
}

void ConfigurationClass::deleteInverterById(const uint8_t id)
{
    config.Inverter[id].Serial = 0ULL;
    strlcpy(config.Inverter[id].Name, "", sizeof(config.Inverter[id].Name));
    config.Inverter[id].Order = 0;

    config.Inverter[id].Poll_Enable_Day = true;
    config.Inverter[id].Poll_Enable_Night = true;
    config.Inverter[id].Command_Enable_Day = true;
    config.Inverter[id].Command_Enable_Night = true;
    config.Inverter[id].ReachableThreshold = REACHABLE_THRESHOLD;
    config.Inverter[id].ZeroRuntimeDataIfUnrechable = false;
    config.Inverter[id].ZeroYieldDayOnMidnight = false;
    config.Inverter[id].YieldDayCorrection = false;

    for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
        config.Inverter[id].channel[c].MaxChannelPower = 0;
        config.Inverter[id].channel[c].YieldTotalOffset = 0.0f;
        strlcpy(config.Inverter[id].channel[c].Name, "", sizeof(config.Inverter[id].channel[c].Name));
    }
}

ConfigurationClass Configuration;
