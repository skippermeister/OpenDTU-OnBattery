// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include "Battery.h"
#include "Datastore.h"
#include "Huawei_can.h"
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "PowerMeter.h"
#include "REFUsolRS485Receiver.h"
#include "Utils.h"
#include "VictronMppt.h"
#include "WebApi.h"
#include "defaults.h"
#include <AsyncJson.h>

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on(HttpLink, HTTP_GET, std::bind(&WebApiWsLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();

    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("live websocket");
    reload();
}

void WebApiWsLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) { return; }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebApiWsLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsLiveClass::generateOnBatteryJsonResponse(JsonVariant& root, bool all)
{
    auto const& config = Configuration.get();
    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;

    auto victronAge = VictronMppt.getDataAgeMillis();
    if (all || (victronAge > 0 && (millis() - _lastPublishVictron) > victronAge)) {
        auto vedirectObj = root["vedirect"].to<JsonObject>();
        vedirectObj["enabled"] = config.Vedirect.Enabled;

        if (config.Vedirect.Enabled) {
            auto totalVeObj = vedirectObj["total"].to<JsonObject>();

            addTotalField(totalVeObj, "Power", VictronMppt.getPanelPowerWatts(), "W", 1);
            addTotalField(totalVeObj, "YieldDay", VictronMppt.getYieldDay() * 1000, "Wh", 0);
            addTotalField(totalVeObj, "YieldTotal", VictronMppt.getYieldTotal(), "kWh", 2);
        }

        if (!all) { _lastPublishVictron = millis(); }
    }

#ifdef USE_CHARGER_HUAWEI
    if (all || (HuaweiCan.getLastUpdate() - _lastPublishCharger) < halfOfAllMillis) {
        auto chargerObj = root["charger"].to<JsonObject>();
        chargerObj["enabled"] = config.Huawei.Enabled;
        chargerObj["type"] = "huawei";

        if (config.Huawei.Enabled) {
            const RectifierParameters_t* rp = HuaweiCan.get();
            addTotalField(chargerObj, "Power", rp->input_power, "W", 2);
        }
#endif
#ifdef USE_CHARGER_MEANWELL
    if (all || (MeanWellCan.getLastUpdate() - _lastPublishCharger) < halfOfAllMillis) {
        auto chargerObj = root["charger"].to<JsonObject>();
        chargerObj["enabled"] = config.MeanWell.Enabled;
        chargerObj["type"] = "meanwell";

        if (config.MeanWell.Enabled) {
            addTotalField(chargerObj, "Power", MeanWellCan._rp.inputPower, "W", 2);
        }
#endif

        if (!all) { _lastPublishCharger = millis(); }
    }

    auto spStats = Battery.getStats();
    if (all || spStats->updateAvailable(_lastPublishBattery)) {
        auto batteryObj = root["battery"].to<JsonObject>();
        batteryObj["enabled"] = config.Battery.Enabled;

        if (config.Battery.Enabled) {
            if (spStats->isSoCValid()) {
                addTotalField(batteryObj, "soc", spStats->getSoC(), "%", spStats->getSoCPrecision());
            }

            if (spStats->isVoltageValid()) {
                addTotalField(batteryObj, "voltage", spStats->getVoltage(), "V", 2);
            }

            if (spStats->isCurrentValid()) {
                addTotalField(batteryObj, "current", spStats->getChargeCurrent(), "A", spStats->getChargeCurrentPrecision());
            }

            if (spStats->isVoltageValid() && spStats->isCurrentValid()) {
                addTotalField(batteryObj, "power", spStats->getVoltage() * spStats->getChargeCurrent(), "W", 1);
            }
        }

        if (!all) { _lastPublishBattery = millis(); }
    }

    if (all || (PowerMeter.getLastUpdate() - _lastPublishPowerMeter) < halfOfAllMillis) {
        auto powerMeterObj = root["power_meter"].to<JsonObject>();
        powerMeterObj["enabled"] = config.PowerMeter.Enabled;

        if (config.PowerMeter.Enabled) {
            addTotalField(powerMeterObj, "GridPower", PowerMeter.getPowerTotal(), "W", 1);
            addTotalField(powerMeterObj, "HousePower", PowerMeter.getHousePower(), "W", 1);
        }

        if (!all) { _lastPublishPowerMeter = millis(); }
    }

#if defined(USE_REFUsol_INVERTER)
    auto refusolObj = root["refusol"].to<JsonObject>();
    refusolObj["enabled"] = config.REFUsol.Enabled;
    auto totalREFUsolObj = refusolObj["total"].to<JsonObject>();

    if (config.REFUsol.Enabled) {
        addTotalField(totalREFUsolObj, "Power", REFUsol.Frame.acPower, "W", 1);
        addTotalField(totalREFUsolObj, "YieldDay", REFUsol.Frame.YieldDay * 1000, "kWh", 3);
        addTotalField(totalREFUsolObj, "YieldTotal", REFUsol.Frame.YieldTotal, "kWh", 3);
    }
#endif

    if (all || (millis() - _lastPublishHours) > 10 * 1000) { // update every 10 seconds
        // Daten visualisieren #168
        addHourPower(root, Datastore.getHourlyPowerData(), "Wh", Datastore.getTotalAcYieldTotalDigits());
        _lastPublishHours = millis();
    }
}

void WebApiWsLiveClass::sendOnBatteryStats()
{
    JsonDocument root;
    JsonVariant var = root;

    bool all = (millis() - _lastPublishOnBatteryFull) > 10 * 1000;
    if (all) { _lastPublishOnBatteryFull = millis(); }
    generateOnBatteryJsonResponse(var, all);

    if (root.isNull()) { return; }

    if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
       String buffer;
        serializeJson(root, buffer);

        _ws.textAll(buffer);
    }
}

void WebApiWsLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    sendOnBatteryStats();

    // Loop all inverters
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        const uint32_t lastUpdateInternal = inv->Statistics()->getLastUpdateFromInternal();
        if (!((lastUpdateInternal > 0 && lastUpdateInternal > _lastPublishStats[i]) || (millis() - _lastPublishStats[i] > (10 * 1000)))) {
            continue;
        }

        _lastPublishStats[i] = millis();

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            JsonDocument root;
            JsonVariant var = root;

            auto invArray = var["inverters"].to<JsonArray>();
            auto invObject = invArray.add<JsonObject>();

            generateCommonJsonResponse(var);
            generateInverterCommonJsonResponse(invObject, inv);
            generateInverterChannelJsonResponse(invObject, inv);

            if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
                continue;
            }

            String buffer;
            serializeJson(root, buffer);
            // Serial.println(buffer);
            _ws.textAll(buffer);

        } catch (const std::bad_alloc& bad_alloc) {
            MessageOutput.printf("Calling %s temporarily out of resources. Reason: \"%s\".\r\n", HttpLink, bad_alloc.what());
        } catch (const std::exception& exc) {
            MessageOutput.printf("Unknown exception in %s. Reason: \"%s\".\r\n", HttpLink, exc.what());
        }
    }
}

void WebApiWsLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    auto totalObj = root["total"].to<JsonObject>();;
    addTotalField(totalObj, "Power", Datastore.getTotalAcPowerEnabled(), "W", Datastore.getTotalAcPowerDigits());
    addTotalField(totalObj, "YieldDay", Datastore.getTotalAcYieldDayEnabled(), "Wh", Datastore.getTotalAcYieldDayDigits());
    addTotalField(totalObj, "YieldTotal", Datastore.getTotalAcYieldTotalEnabled(), "kWh", Datastore.getTotalAcYieldTotalDigits());

    JsonObject hintObj = root["hints"].to<JsonObject>();
    struct tm timeinfo;
    hintObj["time_sync"] = !getLocalTime(&timeinfo, 5);
    hintObj["radio_problem"] =
#ifdef USE_RADIO_NRF
            ( Hoymiles.getRadioNrf()->isInitialized() &&
            ( !Hoymiles.getRadioNrf()->isConnected() || !Hoymiles.getRadioNrf()->isPVariant())
            )
#endif
#if defined(USE_RADIO_NRF) && defined(USE_RADIO_CMT)
            ||
#endif
#ifdef USE_RADIO_CMT
            ( Hoymiles.getRadioCmt()->isInitialized() && !Hoymiles.getRadioCmt()->isConnected() )
#endif
        ;

    hintObj["default_password"] = strcmp(Configuration.get().Security.Password, ACCESS_POINT_PASSWORD) == 0;
}

void WebApiWsLiveClass::generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    root["serial"] = inv->serialString();
    root["name"] = inv->name();
    root["order"] = inv_cfg->Order;
    root["data_age"] = (millis() - inv->Statistics()->getLastUpdate()) / 1000;
    root["poll_enabled"] = inv->getEnablePolling();
    root["reachable"] = inv->isReachable();
    root["producing"] = inv->isProducing();
    root["limit_relative"] = inv->SystemConfigPara()->getLimitPercent();
    if (inv->DevInfo()->getMaxPower() > 0) {
        root["limit_absolute"] = inv->SystemConfigPara()->getLimitPercent() * inv->DevInfo()->getMaxPower() / 100.0;
    } else {
        root["limit_absolute"] = -1;
    }
    root["radio_stats"]["tx_request"] = inv->RadioStats.TxRequestData;
    root["radio_stats"]["tx_re_request"] = inv->RadioStats.TxReRequestFragment;
    root["radio_stats"]["rx_success"] = inv->RadioStats.RxSuccess;
    root["radio_stats"]["rx_fail_nothing"] = inv->RadioStats.RxFailNoAnswer;
    root["radio_stats"]["rx_fail_partial"] = inv->RadioStats.RxFailPartialAnswer;
    root["radio_stats"]["rx_fail_corrupt"] = inv->RadioStats.RxFailCorruptData;
    root["radio_stats"]["rssi"] = inv->getLastRssi();
}

void WebApiWsLiveClass::generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    // Loop all channels
    for (auto& t : inv->Statistics()->getChannelTypes()) {
        auto chanTypeObj = root[inv->Statistics()->getChannelTypeName(t)].to<JsonObject>();
        for (auto& c : inv->Statistics()->getChannelsByType(t)) {
            if (t == TYPE_DC) {
                chanTypeObj[String(static_cast<uint8_t>(c))]["name"]["u"] = inv_cfg->channel[c].Name;
            }
            addField(chanTypeObj, inv, t, c, FLD_PAC);
            addField(chanTypeObj, inv, t, c, FLD_UAC);
            addField(chanTypeObj, inv, t, c, FLD_IAC);
            if (t == TYPE_INV) {
                addField(chanTypeObj, inv, t, c, FLD_PDC, "Power DC");
            } else {
                addField(chanTypeObj, inv, t, c, FLD_PDC);
            }
            addField(chanTypeObj, inv, t, c, FLD_UDC);
            addField(chanTypeObj, inv, t, c, FLD_IDC);
            addField(chanTypeObj, inv, t, c, FLD_YD);
            addField(chanTypeObj, inv, t, c, FLD_YT);
            addField(chanTypeObj, inv, t, c, FLD_F);
            addField(chanTypeObj, inv, t, c, FLD_T);
            addField(chanTypeObj, inv, t, c, FLD_PF);
            addField(chanTypeObj, inv, t, c, FLD_Q);
            addField(chanTypeObj, inv, t, c, FLD_EFF);
            if (t == TYPE_DC && inv->Statistics()->getStringMaxPower(c) > 0) {
                addField(chanTypeObj, inv, t, c, FLD_IRR);
                chanTypeObj[String(c)][inv->Statistics()->getChannelFieldName(t, c, FLD_IRR)]["max"] = inv->Statistics()->getStringMaxPower(c);
            }
        }
    }

    if (inv->Statistics()->hasChannelFieldValue(TYPE_INV, CH0, FLD_EVT_LOG)) {
        root["events"] = inv->EventLog()->getEntryCount();
    } else {
        root["events"] = -1;
    }
}

void WebApiWsLiveClass::addField(JsonObject& root, std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic)
{
    if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
        String chanName;
        if (topic == "") {
            chanName = inv->Statistics()->getChannelFieldName(type, channel, fieldId);
        } else {
            chanName = topic;
        }
        String chanNum;
        chanNum = channel;
        root[chanNum][chanName]["v"] = inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
        root[chanNum][chanName]["u"] = inv->Statistics()->getChannelFieldUnit(type, channel, fieldId);
        root[chanNum][chanName]["d"] = inv->Statistics()->getChannelFieldDigits(type, channel, fieldId);
    }
}

void WebApiWsLiveClass::addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits)
{
    root[name]["v"] = value;
    root[name]["u"] = unit;
    root[name]["d"] = digits;
}

// Daten visualisieren #168
void WebApiWsLiveClass::addHourPower(JsonVariant& root, std::array<float, 24> values, const String& unit, const uint8_t digits)
{
    auto hours = root["hours"].to<JsonObject>();
    auto valuesArray = hours["values"].to<JsonArray>();

    for (float value : values) {
        if (value < 0.0)
            value = 0.0;
        valuesArray.add(static_cast<uint16_t>(value));
    }

    hours["u"] = unit;
    hours["d"] = digits;
}

void WebApiWsLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] connect\r\n", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        MessageOutput.printf("Websocket: [%s][%u] disconnect\r\n", server->url(), client->id());
    }
}

void WebApiWsLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        auto invArray = root["inverters"].to<JsonArray>();
        auto serial = WebApi.parseSerialFromRequest(request);

        if (serial > 0) {
            auto inv = Hoymiles.getInverterBySerial(serial);
            if (inv != nullptr) {
                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
                generateInverterChannelJsonResponse(invObject, inv);
            }
        } else {
            // Loop all inverters
            for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
                auto inv = Hoymiles.getInverterByPos(i);
                if (inv == nullptr) {
                    continue;
                }

                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
            }
        }

        generateCommonJsonResponse(root);

        generateOnBatteryJsonResponse(root, true);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (const std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling %s temporarily out of resources. Reason: \"%s\".\r\n", HttpLink, bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        MessageOutput.printf("Unknown exception in %s. Reason: \"%s\".\r\n", HttpLink, exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
