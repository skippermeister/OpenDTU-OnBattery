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

void WebApiPowerMeterClass::onStatus(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 3 * 1024);
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

    auto httpPhases = root.createNestedArray("http_phases");

    for (uint8_t i = 0; i < POWERMETER_MAX_PHASES; i++) {
        JsonObject phaseObject = httpPhases.createNestedObject();

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
        phaseObject["unit"] = cPM.Http.Phase[i].Unit;
    }

    response->setLength();
    request->send(response);
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
    auto& retMsg = response->getRoot();
    retMsg["type"] = Warning;

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > 4096) {
        retMsg["message"] = DataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(4096);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled") && root.containsKey("source"))) {
        retMsg["message"] = ValuesAreMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["source"].as<uint8_t>() == PowerMeter.SOURCE_HTTP) {
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
                    response->setLength();
                    request->send(response);
                    return;
                }

                if ((phase["auth_type"].as<Auth>() != Auth::none)
                    && (phase["username"].as<String>().length() == 0 || phase["password"].as<String>().length() == 0)) {
                    retMsg["message"] = "Username or password must not be empty!";
                    response->setLength();
                    request->send(response);
                    return;
                }

                if (!phase.containsKey("timeout")
                    || phase["timeout"].as<uint16_t>() <= 0) {
                    retMsg["message"] = "Timeout must be greater than 0 ms!";
                    response->setLength();
                    request->send(response);
                    return;
                }

                if (!phase.containsKey("unit")
                    || phase["unit"].as<PowerMeterUnits>() < kW || phase["unit"].as<PowerMeterUnits>() > mW) {
                    retMsg["message"] = "Unit must be kW, W or mW!";
                    response->setLength();
                    request->send(response);
                    return;
                }
            }

            if (!phase.containsKey("json_path")
                || phase["json_path"].as<String>().length() == 0) {
                retMsg["message"] = "Json path must not be empty!";
                response->setLength();
                request->send(response);
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
        JsonObject phase = http_phases[i].as<JsonObject>();

        cPM.Http.Phase[i].Enabled = (i == 0 ? true : phase["enabled"].as<bool>());
        strlcpy(cPM.Http.Phase[i].Url, phase["url"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].Url));
        cPM.Http.Phase[i].AuthType = phase["auth_type"].as<Auth>();
        strlcpy(cPM.Http.Phase[i].Username, phase["username"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].Username));
        strlcpy(cPM.Http.Phase[i].Password, phase["password"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].Password));
        strlcpy(cPM.Http.Phase[i].HeaderKey, phase["header_key"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].HeaderKey));
        strlcpy(cPM.Http.Phase[i].HeaderValue, phase["header_value"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].HeaderValue));
        cPM.Http.Phase[i].Timeout = phase["timeout"].as<uint16_t>();
        strlcpy(cPM.Http.Phase[i].JsonPath, phase["json_path"].as<String>().c_str(), sizeof(cPM.Http.Phase[i].JsonPath));
        cPM.Http.Phase[i].Unit = phase["unit"].as<PowerMeterUnits>();
    }

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

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
    auto& retMsg = asyncJsonResponse->getRoot();
    retMsg["type"] = Warning;

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        asyncJsonResponse->setLength();
        request->send(asyncJsonResponse);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > 2 * 1024) {
        retMsg["message"] = DataTooLarge;
        asyncJsonResponse->setLength();
        request->send(asyncJsonResponse);
        return;
    }

    DynamicJsonDocument root(2 * 1024);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        asyncJsonResponse->setLength();
        request->send(asyncJsonResponse);
        return;
    }

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
    if (HttpPowerMeter.queryPhase(phase, root["url"].as<String>().c_str(),
            root["auth_type"].as<Auth>(), root["username"].as<String>().c_str(), root["password"].as<String>().c_str(),
            root["header_key"].as<String>().c_str(), root["header_value"].as<String>().c_str(), root["timeout"].as<uint16_t>(),
            root["json_path"].as<String>().c_str())) {
        retMsg["type"] = Success;
        snprintf(response, sizeof(response), "Success! Power: %5.2fW", HttpPowerMeter.getPower(phase + 1));
    } else {
        snprintf(response, sizeof(response), "%s", HttpPowerMeter.httpPowerMeterError);
    }

    retMsg["message"] = response;
    asyncJsonResponse->setLength();
    request->send(asyncJsonResponse);
}
