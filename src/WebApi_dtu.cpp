// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_dtu.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

WebApiDtuClass::WebApiDtuClass()
    : _applyDataTask(TASK_IMMEDIATE, TASK_ONCE, std::bind(&WebApiDtuClass::applyDataTaskCb, this))
{
}

void WebApiDtuClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/dtu/config", HTTP_GET, std::bind(&WebApiDtuClass::onDtuAdminGet, this, _1));
    server.on("/api/dtu/config", HTTP_POST, std::bind(&WebApiDtuClass::onDtuAdminPost, this, _1));

    scheduler.addTask(_applyDataTask);
}

void WebApiDtuClass::applyDataTaskCb()
{
    // Execute stuff in main thread to avoid busy SPI bus
    CONFIG_T& config = Configuration.get();
    Hoymiles.getRadioNrf()->setPALevel((rf24_pa_dbm_e)config.Dtu.Nrf.PaLevel);
#ifdef USE_RADIO_CMT
    Hoymiles.getRadioCmt()->setPALevel(config.Dtu.Cmt.PaLevel);
#endif
    Hoymiles.getRadioNrf()->setDtuSerial(config.Dtu.Serial);
#ifdef USE_RADIO_CMT
    Hoymiles.getRadioCmt()->setDtuSerial(config.Dtu.Serial);
    Hoymiles.getRadioCmt()->setCountryMode(static_cast<CountryModeId_t>(config.Dtu.Cmt.CountryMode));
    Hoymiles.getRadioCmt()->setInverterTargetFrequency(config.Dtu.Cmt.Frequency);
#endif
    Hoymiles.setPollInterval(config.Dtu.PollInterval);
}

void WebApiDtuClass::onDtuAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const Dtu_CONFIG_T& cDtu = Configuration.get().Dtu;

    // DTU Serial is read as HEX
    char buffer[sizeof(uint64_t) * 8 + 1];
    snprintf(buffer, sizeof(buffer), "%0x%08x",
        ((uint32_t)((cDtu.Serial >> 32) & 0xFFFFFFFF)),
        ((uint32_t)(cDtu.Serial & 0xFFFFFFFF)));
    root["serial"] = buffer;
    root["pollinterval"] = cDtu.PollInterval;
    root["nrf_enabled"] = Hoymiles.getRadioNrf()->isInitialized();
    root["nrf_palevel"] = cDtu.Nrf.PaLevel;
#ifdef USE_RADIO_CMT
    root["cmt_enabled"] = Hoymiles.getRadioCmt()->isInitialized();
    root["cmt_palevel"] = cDtu.Cmt.PaLevel;
    root["cmt_frequency"] = cDtu.Cmt.Frequency;
    root["cmt_country"] = cDtu.Cmt.CountryMode;
    root["cmt_chan_width"] = Hoymiles.getRadioCmt()->getChannelWidth();

    auto data = root.createNestedArray("country_def");
    auto countryDefs = Hoymiles.getRadioCmt()->getCountryFrequencyList();
    for (const auto& definition : countryDefs) {
        auto obj = data.createNestedObject();
        obj["freq_default"] = definition.definition.Freq_Default;
        obj["freq_min"] = definition.definition.Freq_Min;
        obj["freq_max"] = definition.definition.Freq_Max;
        obj["freq_legal_min"] = definition.definition.Freq_Legal_Min;
        obj["freq_legal_max"] = definition.definition.Freq_Legal_Max;
    }
#else
    root["cmt_enabled"] = false;
#endif

    root["verbose_logging"] = Hoymiles.getVerboseLogging();

    response->setLength();
    request->send(response);
}

void WebApiDtuClass::onDtuAdminPost(AsyncWebServerRequest* request)
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

    const String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("serial")
            && root.containsKey("pollinterval")
            && root.containsKey("verbose_logging")
#ifdef USE_RADIO_CMT
            && root.containsKey("cmt_palevel")
            && root.containsKey("cmt_frequency")
            && root.containsKey("cmt_country")
#endif
            && root.containsKey("nrf_palevel"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    // Interpret the string as a hex value and convert it to uint64_t
    const uint64_t serial = strtoll(root["serial"].as<String>().c_str(), NULL, 16);

    if (serial == 0) {
        retMsg["message"] = "Serial cannot be zero!";
        retMsg["code"] = WebApiError::DtuSerialZero;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["pollinterval"].as<uint32_t>() == 0) {
        retMsg["message"] = "Poll interval must be greater zero!";
        retMsg["code"] = WebApiError::DtuPollZero;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["nrf_palevel"].as<uint8_t>() > 3) {
        retMsg["message"] = InvalidPowerLevelSetting;
        retMsg["code"] = WebApiError::DtuInvalidPowerLevel;
        response->setLength();
        request->send(response);
        return;
    }

#ifdef USE_RADIO_CMT
    if (root["cmt_palevel"].as<int8_t>() < -10 || root["cmt_palevel"].as<int8_t>() > 20) {
        retMsg["message"] = InvalidPowerLevelSetting;
        retMsg["code"] = WebApiError::DtuInvalidPowerLevel;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["cmt_country"].as<uint8_t>() >= CountryModeId_t::CountryModeId_Max) {
        retMsg["message"] = "Invalid country setting!";
        retMsg["code"] = WebApiError::DtuInvalidCmtCountry;
        response->setLength();
        request->send(response);
        return;
    }

    auto FrequencyDefinition = Hoymiles.getRadioCmt()->getCountryFrequencyList()[root["cmt_country"].as<CountryModeId_t>()].definition;
    if (root["cmt_frequency"].as<uint32_t>() < FrequencyDefinition.Freq_Min
        || root["cmt_frequency"].as<uint32_t>() > FrequencyDefinition.Freq_Max
        || root["cmt_frequency"].as<uint32_t>() % Hoymiles.getRadioCmt()->getChannelWidth() > 0) {

        retMsg["message"] = "Invalid CMT frequency setting!";
        retMsg["code"] = WebApiError::DtuInvalidCmtFrequency;
        retMsg["param"]["min"] = FrequencyDefinition.Freq_Min;
        retMsg["param"]["max"] = FrequencyDefinition.Freq_Max;
        response->setLength();
        request->send(response);
        return;
    }
#endif

    Dtu_CONFIG_T& cDtu = Configuration.get().Dtu;

    cDtu.Serial = serial;
    cDtu.PollInterval = root["pollinterval"].as<uint32_t>();
    cDtu.Nrf.PaLevel = root["nrf_palevel"].as<uint8_t>();
#ifdef USE_RADIO_CMT
    cDtu.Cmt.PaLevel = root["cmt_palevel"].as<int8_t>();
    cDtu.Cmt.Frequency = root["cmt_frequency"].as<uint32_t>();
    cDtu.Cmt.CountryMode = root["cmt_country"].as<CountryModeId_t>();
#endif
    Hoymiles.setVerboseLogging(root["verbose_logging"].as<bool>());

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    _applyDataTask.enable();
}
