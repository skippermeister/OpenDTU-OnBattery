// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_powermeter.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "ErrorMessages.h"
#include "HttpPowerMeter.h"
#include "MqttHandleHass.h"
#include "MqttHandleVedirectHass.h"
#include "MqttSettings.h"
#include "PowerLimiter.h"
#include "PowerMeter.h"
#include "REFUsolRS485Receiver.h"
#include "VeDirectFrameHandler.h"
#include "WebApi.h"
#include "helper.h"

void WebApiPowerMeterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/powermeter/status", HTTP_GET, std::bind(&WebApiPowerMeterClass::onStatus, this, _1));
    server.on("/api/powermeter/config", HTTP_GET, std::bind(&WebApiPowerMeterClass::onAdminGet, this, _1));
    server.on("/api/powermeter/config", HTTP_POST, std::bind(&WebApiPowerMeterClass::onAdminPost, this, _1));
    server.on("/api/powermeter/testhttprequest", HTTP_POST, std::bind(&WebApiPowerMeterClass::onTestHttpRequest, this, _1));
}

void WebApiPowerMeterClass::decodeJsonPhaseConfig(JsonObject const& json, PowerMeterHttpConfig& config) const
{
        config.Enabled = json["enabled"].as<bool>();
        strlcpy(config.Url, json["url"].as<String>().c_str(), sizeof(config.Url));
        config.AuthType = json["auth_type"].as<PowerMeterHttpConfig::Auth>();
        strlcpy(config.Username, json["username"].as<String>().c_str(), sizeof(config.Username));
        strlcpy(config.Password, json["password"].as<String>().c_str(), sizeof(config.Password));
        strlcpy(config.HeaderKey, json["header_key"].as<String>().c_str(), sizeof(config.HeaderKey));
        strlcpy(config.HeaderValue, json["header_value"].as<String>().c_str(), sizeof(config.HeaderValue));
        config.Timeout = json["timeout"].as<uint16_t>();
        strlcpy(config.JsonPath, json["json_path"].as<String>().c_str(), sizeof(config.JsonPath));
        config.PowerUnit = json["unit"].as<PowerMeterHttpConfig::Unit>();
        config.SignInverted = json["sign_inverted"].as<bool>();
}

void WebApiPowerMeterClass::onStatus(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    root["enabled"] = cPM.Enabled;
    root["source"] = cPM.Source;
    root["pollinterval"] = cPM.PollInterval;
    root["updatesonly"] = cPM.UpdatesOnly;
    root["verbose_logging"] = PowerMeter.getVerboseLogging();
    root["mqtt_topic_powermeter_1"] = cPM.Mqtt.TopicPowerMeter1;
    root["mqtt_topic_powermeter_2"] = cPM.Mqtt.TopicPowerMeter2;
    root["mqtt_topic_powermeter_3"] = cPM.Mqtt.TopicPowerMeter3;
    root["sdmbaudrate"] = cPM.Sdm.Baudrate;
    root["sdmaddress"] = cPM.Sdm.Address;
    root["http_individual_requests"] = cPM.Http.IndividualRequests;

    auto httpPhases = root["http_phases"].to<JsonArray>();

    for (uint8_t i = 0; i < POWERMETER_MAX_PHASES; i++) {
        JsonObject phaseObject = httpPhases.add<JsonObject>();

        phaseObject["index"] = i + 1;
        phaseObject["enabled"] = cPM.Http.Phase[i].Enabled;
        phaseObject["url"] = String(cPM.Http.Phase[i].Url);
        phaseObject["auth_type"] = cPM.Http.Phase[i].AuthType;
        phaseObject["username"] = String(cPM.Http.Phase[i].Username);
        phaseObject["password"] = String(cPM.Http.Phase[i].Password);
        phaseObject["header_key"] = String(cPM.Http.Phase[i].HeaderKey);
        phaseObject["header_value"] = String(cPM.Http.Phase[i].HeaderValue);
        phaseObject["json_path"] = String(cPM.Http.Phase[i].JsonPath);
        phaseObject["timeout"] = cPM.Http.Phase[i].Timeout;
        phaseObject["unit"] = cPM.Http.Phase[i].PowerUnit;
        phaseObject["sign_inverted"] = cPM.Http.Phase[i].SignInverted;
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiPowerMeterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onStatus(request);
}

void WebApiPowerMeterClass::onAdminPost(AsyncWebServerRequest* request)
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

    if (!(root.containsKey("enabled") && root.containsKey("source"))) {
        retMsg["message"] = ValuesAreMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (static_cast<PowerMeterClass::Source>(root["source"].as<uint8_t>()) == PowerMeterClass::Source::HTTP) {
        JsonArray http_phases = root["http_phases"];
        for (uint8_t i = 0; i < http_phases.size(); i++) {
            JsonObject phase = http_phases[i].as<JsonObject>();

            if (i > 0 && !phase["enabled"].as<bool>()) {
                continue;
            }

            if (i == 0 || phase["http_individual_requests"].as<bool>()) {
                if (!phase.containsKey("url")
                    || (!phase["url"].as<String>().startsWith("http://")
                        && !phase["url"].as<String>().startsWith("https://"))) {
                    retMsg["message"] = "URL must either start with http:// or https://!";
                    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                    return;
                }

                if ((phase["auth_type"].as<uint8_t>() != PowerMeterHttpConfig::Auth::None)
                    && (phase["username"].as<String>().length() == 0 || phase["password"].as<String>().length() == 0)) {
                    retMsg["message"] = "Username or password must not be empty!";
                    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                    return;
                }

                if (!phase.containsKey("timeout")
                    || phase["timeout"].as<uint16_t>() <= 0) {
                    retMsg["message"] = "Timeout must be greater than 0 ms!";
                    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                    return;
                }

                if (!phase.containsKey("unit")
                    || phase["unit"].as<PowerMeterHttpConfig::Unit>() < PowerMeterHttpConfig::Unit::KiloWatts || phase["unit"].as<PowerMeterHttpConfig::Unit>() > PowerMeterHttpConfig::Unit::MilliWatts) {
                    retMsg["message"] = "Unit must be kW, W or mW!";
                    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                    return;
                }
            }

            if (!phase.containsKey("json_path")
                || phase["json_path"].as<String>().length() == 0) {
                retMsg["message"] = "Json path must not be empty!";
                WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
                return;
            }
        }
    }

    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    bool performRestart = false;
    if (cPM.Source != root["source"].as<uint8_t>())
        performRestart = true;

    cPM.Enabled = root["enabled"].as<bool>();
    PowerMeter.setVerboseLogging(root["verbose_logging"].as<bool>());
#if defined (USE_SMA_HM)
    SMA_HM.setVerboseLogging(root["verbose_logging"].as<bool>());
#endif
    cPM.Source = root["source"].as<uint8_t>();
    cPM.PollInterval = root["pollinterval"].as<uint32_t>();
    cPM.UpdatesOnly = root["updatesonly"].as<uint32_t>();
    strlcpy(cPM.Mqtt.TopicPowerMeter1, root["mqtt_topic_powermeter_1"].as<String>().c_str(), sizeof(cPM.Mqtt.TopicPowerMeter1));
    strlcpy(cPM.Mqtt.TopicPowerMeter2, root["mqtt_topic_powermeter_2"].as<String>().c_str(), sizeof(cPM.Mqtt.TopicPowerMeter2));
    strlcpy(cPM.Mqtt.TopicPowerMeter3, root["mqtt_topic_powermeter_3"].as<String>().c_str(), sizeof(cPM.Mqtt.TopicPowerMeter3));
    cPM.Sdm.Baudrate = root["sdmbaudrate"].as<uint32_t>();
    cPM.Sdm.Address = root["sdmaddress"].as<uint8_t>();
    cPM.Http.IndividualRequests = root["http_individual_requests"].as<bool>();

    auto http_phases = root["http_phases"];
    for (uint8_t i = 0; i < http_phases.size(); i++) {
        decodeJsonPhaseConfig(http_phases[i].as<JsonObject>(), cPM.Http.Phase[i]);
    }
    cPM.Http.Phase[0].Enabled = true;

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    yield();

    if (performRestart) {
        delay(1000);
        yield();
        ESP.restart();
    }
}

void WebApiPowerMeterClass::onTestHttpRequest(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* asyncJsonResponse = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, asyncJsonResponse, root)) {
        return;
    }

    auto& retMsg = asyncJsonResponse->getRoot();

    if (!root.containsKey("url")
        || !root.containsKey("auth_type")
        || !root.containsKey("username")
        || !root.containsKey("password")
        || !root.containsKey("header_key")
        || !root.containsKey("header_value")
        || !root.containsKey("timeout")
        || !root.containsKey("json_path")
        || !root.containsKey("unit")) {
        retMsg["message"] = MissingFields;
        asyncJsonResponse->setLength();
        request->send(asyncJsonResponse);
        return;
    }

    char response[256];

    int phase = 0; //"absuing" index 0 of the float power[3] in HttpPowerMeter to store the result
    PowerMeterHttpConfig phaseConfig;
    decodeJsonPhaseConfig(root.as<JsonObject>(), phaseConfig);
    if (HttpPowerMeter.queryPhase(phase, phaseConfig)) {
        retMsg["type"] = Success;
        snprintf(response, sizeof(response), "Success! Power: %5.2fW", HttpPowerMeter.getPower(phase + 1));
    } else {
        snprintf(response, sizeof(response), "%s", HttpPowerMeter.httpPowerMeterError);
    }

    retMsg["message"] = response;
    asyncJsonResponse->setLength();
    request->send(asyncJsonResponse);
}
