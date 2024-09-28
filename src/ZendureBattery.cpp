#ifdef USE_MQTT_ZENDURE_BATTERY

#include <functional>

#include "Configuration.h"
#include "ZendureBattery.h"
#include "MqttSettings.h"
#include "MessageOutput.h"
#include "Utils.h"

bool ZendureBattery::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;

    auto const& config = Configuration.get();
    String deviceType;
    String deviceName;

    switch (config.Battery.ZendureDeviceType){
        case 0:
            deviceType = ZENDURE_HUB1200;
            deviceName = String("HUB 1200");
            break;
        case 1:
            deviceType = ZENDURE_HUB2000;
            deviceName = String("HUB 2000");
            break;
        case 2:
            deviceType = ZENDURE_AIO2400;
            deviceName = String("AIO 2400");
            break;
        case 3:
            deviceType = ZENDURE_ACE1500;
            deviceName = String("Ace 1500");
            break;
        case 4:
            deviceType = ZENDURE_HYPER2000;
            deviceName = String("Hyper 2000");
            break;
        default:
            MessageOutput.printf("ZendureBattery: Invalid device type!");
            return false;
    }

    if (strlen(config.Battery.ZendureDeviceId) != 8) {
        MessageOutput.printf("ZendureBattery: Invalid device Id!");
        return false;
    }

    // setup static device info
    _stats->setDevice(std::move(deviceName));

    // store device ID as we will need them for checking when receiving messages
    _deviceId = config.Battery.ZendureDeviceId;

    _baseTopic = "/" + deviceType + "/" + _deviceId + "/";
    _readTopic = "iot" + _baseTopic + "properties/read";
    _writeTopic = "iot" + _baseTopic + "properties/write";

    // subscribe for log messages
    _topicLog = _baseTopic + "log";
    MqttSettings.subscribe(_topicLog, 0/*QoS*/,
            std::bind(&ZendureBattery::onMqttMessageLog,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    if (_verboseLogging) {
        MessageOutput.printf("ZendureBattery: Subscribed to '%s' for status readings\r\n", _topicLog.c_str());
    }

    // subscribe for report messages
    _topicReport = _baseTopic + "properties/report";
    MqttSettings.subscribe(_topicReport, 0/*QoS*/,
            std::bind(&ZendureBattery::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    if (_verboseLogging) {
        MessageOutput.printf("ZendureBattery: Subscribed to '%s' for status readings\r\n", _topicReport.c_str());
    }

#ifndef ZENDURE_NO_REDUCED_UPDATE
    // subscribe for read messages
    _topicReadReply = _baseTopic + "properties/read/reply";
    MqttSettings.subscribe(_topicReadReply, 0/*QoS*/,
            std::bind(&ZendureBattery::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    if (_verboseLogging) {
        MessageOutput.printf("ZendureBattery: Subscribed to '%s' for status readings\r\n", _topicReadReply.c_str());
    }
    _rateUpdateMs       = min(static_cast<uint32_t>(config.Battery.ZendurePollingInterval * 100), 10U * 1000);
    _nextUpdate         = 0;
#endif

    _rateFullUpdateMs   = config.Battery.ZendurePollingInterval * 1000;
    _nextFullUpdate     = 0;
    _rateTimesyncMs     = 3600 * 1000;
    _nextTimesync       = 0;

    // pre-generate the settings request
    JsonDocument root;
    JsonVariant prop = root[ZENDURE_REPORT_PROPERTIES].to<JsonObject>();
    prop[ZENDURE_REPORT_PV_BRAND] = 1; // means Hoymiles
    prop[ZENDURE_REPORT_PV_AUTO_MODEL] = 0; // we did static setup
    prop[ZENDURE_REPORT_AUTO_RECOVER] = static_cast<uint8_t>(config.Battery.ZendureBypassMode == static_cast<uint8_t>(ZendureBatteryStats::BypassMode::Automatic));
    prop[ZENDURE_REPORT_AUTO_SHUTDOWN] = config.Battery.ZendureAutoShutdown;
    prop[ZENDURE_REPORT_BUZZER_SWITCH] = 0; // disable, as it is anoying
    prop[ZENDURE_REPORT_BYPASS_MODE] = config.Battery.ZendureBypassMode;
    prop[ZENDURE_REPORT_MAX_SOC] = config.Battery.ZendureMaxSoC * 10;
    prop[ZENDURE_REPORT_MIN_SOC] = config.Battery.ZendureMinSoC * 10;
    if (config.Battery.ZendureForceLimit){
        prop[ZENDURE_REPORT_OUTPUT_LIMIT] = config.Battery.ZendureMaxOutput;
    }
    prop[ZENDURE_REPORT_INVERSE_MAX_POWER] = config.Battery.ZendureMaxOutput;
    prop[ZENDURE_REPORT_SMART_MODE] = 0; // should be disabled
    serializeJson(root, _payloadSettings);

    // pre-generate the full update request
    root.clear();
    JsonArray array = root[ZENDURE_REPORT_PROPERTIES].to<JsonArray>();
    array.add("getAll");
    array.add("getInfo");
    serializeJson(root, _payloadFullUpdate);

#ifndef ZENDURE_NO_REDUCED_UPDATE
    // pre-generate the partitial update request
    root.clear();
    array = root[ZENDURE_REPORT_PROPERTIES].to<JsonArray>();
    array.add(ZENDURE_REPORT_MIN_SOC);
    array.add(ZENDURE_REPORT_MAX_SOC);
    array.add(ZENDURE_REPORT_INPUT_LIMIT);
    array.add(ZENDURE_REPORT_OUTPUT_LIMIT);
    array.add(ZENDURE_REPORT_INVERSE_MAX_POWER);
    array.add(ZENDURE_REPORT_PACK_STATE);
    array.add(ZENDURE_REPORT_HEAT_STATE);
    array.add(ZENDURE_REPORT_AUTO_SHUTDOWN);
    array.add(ZENDURE_REPORT_BUZZER_SWITCH);
    array.add(ZENDURE_REPORT_REMAIN_OUT_TIME);
    array.add(ZENDURE_REPORT_REMAIN_IN_TIME);
    serializeJson(root, _payloadUpdate);
#endif

    return true;
}

void ZendureBattery::deinit()
{
    if (!_topicReport.isEmpty()) {
        MqttSettings.unsubscribe(_topicReport);
        _topicReport.clear();
    }
    if (!_topicLog.isEmpty()) {
        MqttSettings.unsubscribe(_topicLog);
        _topicLog.clear();
    }
#ifndef ZENDURE_NO_REDUCED_UPDATE
    if (!_topicReadReply.isEmpty()){
        MqttSettings.unsubscribe(_topicReadReply);
        _topicReadReply.clear();
    }
#endif
}

void ZendureBattery::loop()
{
    auto ms = millis();

    if (!_topicRead.isEmpty()) {
        if (!_payloadFullUpdate.isEmpty() && ms >= _nextFullUpdate) {
            _nextFullUpdate = ms + _rateFullUpdateMs;
#ifndef ZENDURE_NO_REDUCED_UPDATE
            _nextUpdate = ms + _rateUpdateMs;
#endif
            MqttSettings.publishGeneric(_topicRead, _payloadFullUpdate, false, 0);
        }
#ifndef ZENDURE_NO_REDUCED_UPDATE
        if (!_payloadUpdate.isEmpty() && ms >= _nextUpdate) {
            _nextUpdate = ms + _rateUpdateMs;
            MqttSettings.publishGeneric(_topicRead, _payloadUpdate, false, 0);
        }
#endif
    }

    if (ms >= _nextTimesync) {
        _nextTimesync = ms + _rateTimesyncMs;
        timesync();

        // republish settings - just to be sure
        if (!_topicWrite.isEmpty() && !_payloadSettings.isEmpty()){
            MqttSettings.publishGeneric(_topicWrite, _payloadSettings, false, 0);
        }
    }
}

uint16_t ZendureBattery::setOutputLimit(uint16_t limit)
{
    if (_topicWrite.isEmpty()) {
        return _stats->_output_limit;
    }

    if (_stats->_output_limit != limit){
        if (limit < 100 && limit != 0){
            uint16_t base = limit / 30U;
            uint16_t remain = (limit % 30U) / 15U;
            limit = 30 * base + 30 * remain;
        }
        MqttSettings.publishGeneric(_topicWrite, "{\"properties\": {\"" ZENDURE_REPORT_OUTPUT_LIMIT "\": " + String(limit) + "} }", false, 0);
    }

    return limit;
}

void ZendureBattery::timesync()
{
    struct tm timeinfo;
    if (!_baseTopic.isEmpty() && getLocalTime(&timeinfo, 5)) {
        char timeStringBuff[50];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%s", &timeinfo);
        MqttSettings.publishGeneric("iot" + _baseTopic + "time-sync/reply","{\"zoneOffset\": \"+00:00\", \"messageId\": 123, \"timestamp\": " + String(timeStringBuff) + "}", false, 0);
    }
}

#ifndef ZENDURE_NO_REDUCED_UPDATE
void ZendureBattery::onMqttMessageRead(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
}
#endif

void ZendureBattery::onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    auto log = [_verboseLogging=_verboseLogging](char const* format, auto&&... args) -> void {
        if (_verboseLogging) {
            MessageOutput.printf("ZendureBattery (Report): ");
            MessageOutput.printf(format, args...);
            MessageOutput.println();
        }
        return;
    };

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        return log("cannot parse payload '%s' as JSON", logValue.c_str());
    }

    if (json.overflowed()) {
        return log("payload too large to process as JSON");
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // messageId has to be set to "123"
    // deviceId has to be set to the configured deviceId
    if (!json["messageId"].as<String>().equals("123")){
        return log("Invalid or missing 'messageId' in '%s'", logValue.c_str());
    }
    if (!json["deviceId"].as<String>().equals(_deviceId)){
        return log("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
    }

    auto props = Utils::getJsonElement<JsonObjectConst>(obj, ZENDURE_REPORT_PROPERTIES, 1);
    if (props.has_value()){
        auto sw_version = Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_FW_VERSION);
        if (sw_version.has_value()){
            _stats->setFwVersion(std::move(parseVersion(*sw_version)));
        }

        auto hw_version = Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_HW_VERSION);
        if (hw_version.has_value()){
            _stats->setHwVersion(std::move(parseVersion(*hw_version)));
        }

        auto soc_max = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MAX_SOC);
        if (soc_max.has_value()){
            *soc_max /= 10;
            if (*soc_max >= 40 && *soc_max <= 100){
                _stats->_soc_max = *soc_max;
            }
        }

        auto soc_min = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MIN_SOC);
        if (soc_min.has_value()){
            *soc_min /= 10;
            if (*soc_min >= 0 && *soc_min <= 60){
                _stats->_soc_min = *soc_min;
            }
        }

        auto input_limit = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INPUT_LIMIT);
        if (input_limit.has_value()){
            _stats->_input_limit = *input_limit;
        }

        // auto output_limit = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_OUTPUT_LIMIT);
        // if (output_limit.has_value()){
        //     _stats->_output_limit = *output_limit;
        // }

        auto inverse_max = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INVERSE_MAX_POWER);
        if (inverse_max.has_value()){
            _stats->_inverse_max = *inverse_max;
        }

        auto state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_PACK_STATE);
        if (state.has_value() && *state <= 2){
            _stats->_state = static_cast<ZendureBatteryStats::State>(*state);
        }

        auto heat_state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_HEAT_STATE);
        if (heat_state.has_value()){
            _stats->_heat_state = static_cast<bool>(*heat_state);
        }

        auto auto_shutdown = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_AUTO_SHUTDOWN);
        if (auto_shutdown.has_value()){
            _stats->_auto_shutdown = static_cast<bool>(*auto_shutdown);
        }

        auto buzzer = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BUZZER_SWITCH);
        if (buzzer.has_value()){
            _stats->_buzzer = static_cast<bool>(*buzzer);
        }

        auto outtime = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_REMAIN_OUT_TIME);
        if (outtime.has_value()){
            _stats->_remain_out_time = *outtime >= ZENDURE_REMAINING_TIME_OVERFLOW ? -1 : *outtime;
        }

        auto intime = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_REMAIN_IN_TIME);
        if (intime.has_value()){
            _stats->_remain_in_time = *intime >= ZENDURE_REMAINING_TIME_OVERFLOW ? -1 : *intime;
        }

        _stats->_lastUpdate = ms;
    }



    // stop processing here, if no pack data found in message
    auto packData = Utils::getJsonElement<JsonArrayConst>(obj, ZENDURE_REPORT_PACK_DATE, 2);
    if (!packData.has_value()){
        return;
    }

    // get serial number related to index only if all packs given in message
    if (_stats->_num_batteries != 0 && (*packData).size() == _stats->_num_batteries){
        for (size_t i = 0 ; i < _stats->_num_batteries ; i++){
            auto serial = Utils::getJsonElement<String>((*packData)[i], ZENDURE_REPORT_SERIAL);
            if (serial.has_value()){
                if (!_stats->addPackData(i+1, *serial).has_value()){
                    log("Invalid or unkown serial '%s' in '%s'", (*serial).c_str(), logValue.c_str());
                }
            }else{
                log("Missing serial of battery pack in '%s'", logValue.c_str());
            }
        }
    }

    // get additional data only if all packs were identified
    if (_stats->_packData.size() == _stats->_num_batteries){
        for (auto packDataJson : *packData){
            auto serial = Utils::getJsonElement<String>(packDataJson, ZENDURE_REPORT_SERIAL);
            auto state = Utils::getJsonElement<uint8_t>(packDataJson, ZENDURE_REPORT_STATE);
            auto version = Utils::getJsonElement<uint32_t>(packDataJson, ZENDURE_REPORT_FW_VERSION);

            // do not waste processing time if nothing to do
            if (!serial.has_value() || !(state.has_value() || version.has_value())){
                continue;
            }

            // find pack data related to serial number
            for (auto [index, entry] : _stats->_packData){
                auto pack = _stats->getPackData(index);
                if (pack.has_value() && (*pack)->_serial == serial){
                    if (state.has_value()){
                        (*pack)->_state = static_cast<ZendureBatteryStats::State>(*state);
                    }

                    if (version.has_value()){
                        (*pack)->setFwVersion(std::move(parseVersion(*version)));
                    }

                    (*pack)->_lastUpdate = ms;

                    // we found the pack we searched for, so terminate loop here
                    break;
                }
            }
        }
    }
}

void ZendureBattery::onMqttMessageLog(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    auto log = [_verboseLogging=_verboseLogging](char const* format, auto&&... args) -> void {
        if (_verboseLogging) {
            MessageOutput.printf("ZendureBattery (Log): ");
            MessageOutput.printf(format, args...);
            MessageOutput.println();
        }
        return;
    };

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        return log("cannot parse payload '%s' as JSON", logValue.c_str());
    }

    if (json.overflowed()) {
        return log("payload too large to process as JSON");
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // deviceId has to be set to the configured deviceId
    // logType has to be set to "2"
    if (!json["deviceId"].as<String>().equals(_deviceId)){
        return log("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
    }
    if (!json["logType"].as<String>().equals("2")){
        return log("Invalid or missing 'v' in '%s'", logValue.c_str());
    }

    auto data = Utils::getJsonElement<JsonObjectConst>(obj, "log", 2);
    if (!data.has_value()){
        return log("Unable to find 'log' in '%s'", logValue.c_str());
    }

    _stats->setSerial(Utils::getJsonElement<String>(*data, ZENDURE_REPORT_SERIAL));

    auto params = Utils::getJsonElement<JsonArrayConst>(*data, "params", 1);
    if (!params.has_value()){
        return log("Unable to find 'params' in '%s'", logValue.c_str());
    }

    auto v = *params;

    uint8_t num = v[ZENDURE_LOG_OFFSET_PACKNUM].as<uint8_t>();
    if (num > 0 && num <= ZENDURE_MAX_PACKS){
        uint16_t soc = 0;
        uint16_t voltage = 0;
        int16_t power = 0;
        int16_t current = 0;
        uint32_t cellMin = UINT32_MAX;
        uint32_t cellMax = 0;
        uint32_t cellAvg = 0;
        uint32_t cellDelta = 0;
        int32_t cellTemp = 0;
        uint16_t capacity = 0;

        for (size_t i = 1 ; i <= num ; i++){
            auto pvol = v[ZENDURE_LOG_OFFSET_PACK_VOLTAGE(i)].as<uint16_t>() * 10;
            auto pcur = v[ZENDURE_LOG_OFFSET_PACK_CURRENT(i)].as<int16_t>();
            auto psoc = v[ZENDURE_LOG_OFFSET_PACK_SOC(i)].as<uint16_t>();

            auto ctmp = v[ZENDURE_LOG_OFFSET_PACK_TEMPERATURE(i)].as<int32_t>();
            auto cmin = v[ZENDURE_LOG_OFFSET_PACK_CELL_MIN(i)].as<uint32_t>() * 10;
            auto cmax = v[ZENDURE_LOG_OFFSET_PACK_CELL_MAX(i)].as<uint32_t>() * 10;
            auto cdel = cmax - cmin;

            auto pack = _stats->getPackData(i);
            if (pack.has_value()){
                auto cavg = pvol / (*pack)->getCellCount();

                (*pack)->_cell_voltage_min = static_cast<uint16_t>(cmin);
                (*pack)->_cell_voltage_max = static_cast<uint16_t>(cmax);
                (*pack)->_cell_voltage_avg = static_cast<uint16_t>(cavg);
                (*pack)->_cell_voltage_spread = static_cast<uint16_t>(cdel);
                (*pack)->_cell_temperature_max = static_cast<int16_t>(ctmp);
                (*pack)->_current = static_cast<float>(pcur) / 10.0;
                (*pack)->_voltage_total = static_cast<float>(pvol) / 1000.0;
                (*pack)->_soc_level = static_cast<float>(psoc) / 10.0;
                (*pack)->_power = static_cast<int16_t>((*pack)->_current * (*pack)->_voltage_total);
                (*pack)->_lastUpdate = ms;

                capacity += (*pack)->getCapacity();
                cellAvg += cavg;
                power += (*pack)->_power;
            }

            cellMin = min(cmin, cellMin);
            cellMax = max(cmax, cellMax);
            cellDelta = max(cdel, cellDelta);
            cellTemp = max(ctmp, cellTemp);

            soc += psoc;
            voltage += pvol;
            current += pcur;
        }

        _stats->_num_batteries = num;
        _stats->setSoC(static_cast<float>(soc) / 10.0 / num, 2, ms);
        //_stats->setVoltage(static_cast<float>(voltage) / 1000 / num, ms);
        _stats->setVoltage(v[ZENDURE_LOG_OFFSET_VOLTAGE].as<float>() / 10.0, ms);
        _stats->setCurrent(static_cast<float>(current) / 10.0, 1, ms);
        _stats->setDischargeCurrentLimit(static_cast<float>(_stats->_inverse_max) / _stats->getVoltage(), ms);
        if (capacity){
            _stats->_capacity = capacity;
        }

        _stats->_auto_recover = static_cast<bool>(v[ZENDURE_LOG_OFFSET_AUTO_RECOVER].as<uint8_t>());
        _stats->_bypass_mode = static_cast<ZendureBatteryStats::BypassMode>(v[ZENDURE_LOG_OFFSET_BYPASS_MODE].as<uint8_t>());
        _stats->_soc_min = v[ZENDURE_LOG_OFFSET_MIN_SOC].as<float>();

        _stats->_solar_power_1 = v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_1].as<uint16_t>();
        _stats->_solar_power_2 = v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_2].as<uint16_t>();
        _stats->_input_power = _stats->_solar_power_1 + _stats->_solar_power_2;

        _stats->_output_limit = static_cast<uint16_t>(v[ZENDURE_LOG_OFFSET_OUTPUT_POWER_LIMIT].as<uint32_t>() / 100);
        //_stats->_input_power = v[ZENDURE_LOG_OFFSET_INPUT_POWER].as<uint16_t>();
        _stats->_output_power = v[ZENDURE_LOG_OFFSET_OUTPUT_POWER].as<uint16_t>();
        _stats->_charge_power = v[ZENDURE_LOG_OFFSET_CHARGE_POWER].as<uint16_t>();
        _stats->_discharge_power = v[ZENDURE_LOG_OFFSET_DISCHARGE_POWER].as<uint16_t>();

        _stats->_cellMinMilliVolt = static_cast<uint16_t>(cellMin);
        _stats->_cellMaxMilliVolt = static_cast<uint16_t>(cellMax);
        _stats->_cellAvgMilliVolt = static_cast<uint16_t>(cellAvg) / num;
        _stats->_cellDeltaMilliVolt = static_cast<uint16_t>(cellDelta);
        _stats->_cellTemperature = static_cast<int16_t>(cellTemp);

        _stats->_lastUpdate = ms;

        calculateEfficiency();
    }
}

String ZendureBattery::parseVersion(uint32_t version) {
    uint8_t major = (version >> 12) & 0xF;
    uint8_t minor = (version >> 8) & 0xF;
    uint8_t bugfix = version & 0xFF;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, bugfix);
    return String(buffer);
}

void ZendureBattery::calculateEfficiency() {
    float in = static_cast<float>(_stats->_input_power);
    float out = static_cast<float>(_stats->_output_power);
    float efficiency = 0.0;

    in += static_cast<float>(_stats->_discharge_power);
    out += static_cast<float>(_stats->_charge_power);

    efficiency = in ? out / in : 0.0;

    if (efficiency <= 1 && efficiency >= 0){
        _stats->_efficiency = efficiency * 100;
    }
}
#endif
