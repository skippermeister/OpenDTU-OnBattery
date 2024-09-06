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

    //_baseTopic = "/73bkTV/sU59jtkw/";
    if (strlen(config.Battery.ZendureDeviceSerial) != 8) {
        MessageOutput.printf("ZendureBattery: Invalid serial number!");
        return false;
    }

    _deviceId = config.Battery.ZendureDeviceSerial;

    _baseTopic = "/" + deviceType + "/" + _deviceId + "/";
    _reportTopic = _baseTopic + "properties/report";
    _readTopic = "iot" + _baseTopic + "properties/read";
    _writeTopic = "iot" + _baseTopic + "properties/write";
    _timesyncTopic = "iot" + _baseTopic + "time-sync/reply";

    MqttSettings.subscribe(_reportTopic, 0/*QoS*/,
            std::bind(&ZendureBattery::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );

    _updateRateMs = 15 * 1000;
    _timesyncRateMs = 60 * 60 * 1000;
    _nextUpdate = 0;
    _nextTimesync = 0;

    _stats->setSerial(_deviceId);
    _stats->setDevice(deviceName);
    _stats->setManufacture("Zendure");

    JsonDocument root;
    JsonVariant prop = root["properties"].to<JsonObject>();

    prop["pvBrand"] = 1;
    prop["autoRecover"] = 1;
    prop["buzzerSwitch"] = 0;
    prop["passMode"] = config.Battery.ZendureBypassMode;
    prop["socSet"] = config.Battery.ZendureMaxSoC * 10;
    prop["minSoc"] = config.Battery.ZendureMinSoC * 10;
    //prop["outputLimit"] = config.Battery.ZendureMaxOutput;
    prop["inverseMaxPower"] = config.Battery.ZendureMaxOutput;
    prop["smartMode"] = 0;
    prop["autoModel"] = 0;

    serializeJson(root, _settingsPayload);

    if (_verboseLogging) {
        MessageOutput.printf("ZendureBattery: Subscribed to '%s' for status readings\r\n", _reportTopic.c_str());
    }

    return true;
}

void ZendureBattery::deinit()
{
    if (!_reportTopic.isEmpty()) {
        MqttSettings.unsubscribe(_reportTopic);
    }
}

void ZendureBattery::loop()
{
    auto ms = millis();

    if (ms >= _nextUpdate) {
        _nextUpdate = ms + _updateRateMs;
        if (!_readTopic.isEmpty()) {
            MqttSettings.publishGeneric(_readTopic, "{\"properties\": [\"getAll\"] }", false, 0);
        }
    }

    if (ms >= _nextTimesync) {
        _nextTimesync = ms + _timesyncRateMs;
        timesync();

        // republish settings - just to be sure
        if (!_writeTopic.isEmpty()) {
            if (!_settingsPayload.isEmpty()){
                MqttSettings.publishGeneric(_writeTopic, _settingsPayload, false, 0);
            }
        }
    }
}
uint16_t ZendureBattery::updateOutputLimit(uint16_t limit)
{
    if (_writeTopic.isEmpty()) {
        return _stats->_output_limit;
    }

    if (_stats->_output_limit != limit){
        if (limit < 100 && limit != 0){
            uint16_t base = limit / 30U;
            uint16_t remain = (limit % 30U) / 15U;
            limit = 30 * base + 30 * remain;
        }
        MqttSettings.publishGeneric(_writeTopic, "{\"properties\": {\"outputLimit\": " + String(limit) + "} }", false, 0);
    }

    return limit;
}

void ZendureBattery::timesync()
{
    if (!_timesyncTopic.isEmpty()) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5)) {
            char timeStringBuff[50];
            strftime(timeStringBuff, sizeof(timeStringBuff), "%s", &timeinfo);
            MqttSettings.publishGeneric(_timesyncTopic,"{\"zoneOffset\": \"+00:00\", \"messageId\": 123, \"timestamp\": " + String(timeStringBuff) + "}", false, 0);
        }
    }
}

void ZendureBattery::onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    auto log = [_verboseLogging=_verboseLogging](char const* format, auto&&... args) -> void {
        if (_verboseLogging) {
            MessageOutput.printf("ZendureBattery: ");
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
    // product has to be set to "solarFlow"
    if (!json["messageId"].as<String>().equals("123")){
        return log("Invalid or missing 'messageId' in '%s'", logValue.c_str());
    }
    if (!json["deviceId"].as<String>().equals(_deviceId)){
        return log("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
    }
    if (!json["product"].as<String>().equals("solarFlow")){
        return log("Invalid or missing 'product' in '%s'", logValue.c_str());
    }

    auto packData = Utils::getJsonElement<JsonArrayConst>(obj, "packData", 2);
    if (packData.has_value()){
        for (JsonObjectConst battery : *packData) {
            auto serial = Utils::getJsonElement<String>(battery, "sn");
            if (serial.has_value() && (*serial).length() == 15){
                _stats->updatePackData(*serial, battery, ms);
            }else{
                log("Invalid or missing serial of battery pack in '%s'", logValue.c_str());
            }

        }
    }

    auto props = Utils::getJsonElement<JsonObjectConst>(obj, "properties", 1);
    if (props.has_value()){
        _stats->update(*props, ms);
    }
}
#endif
