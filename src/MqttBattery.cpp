#ifdef USE_MQTT_BATTERY

#include <functional>

#include "Configuration.h"
#include "MqttBattery.h"
#include "MqttSettings.h"
#include "MessageOutput.h"
#include "Utils.h"

bool MqttBattery::init()
{
    auto const& config = Configuration.get();

    _socTopic = config.Battery.Mqtt.SocTopic;
    if (!_socTopic.isEmpty()) {
        MqttSettings.subscribe(_socTopic, 0/*QoS*/,
                std::bind(&MqttBattery::onMqttMessageSoC,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    config.Battery.Mqtt.SocJsonPath)
                );

        if (_verboseLogging) {
            MessageOutput.printf("MqttBattery: Subscribed to '%s' for SoC readings\r\n",
                _socTopic.c_str());
        }
    }

    _voltageTopic = config.Battery.Mqtt.VoltageTopic;
    if (!_voltageTopic.isEmpty()) {
        MqttSettings.subscribe(_voltageTopic, 0/*QoS*/,
                std::bind(&MqttBattery::onMqttMessageVoltage,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    config.Battery.Mqtt.VoltageJsonPath)
                );

        if (_verboseLogging) {
            MessageOutput.printf("MqttBattery: Subscribed to '%s' for voltage readings\r\n",
                _voltageTopic.c_str());
        }
    }

    _initialized = true;

    return true;
}

void MqttBattery::deinit()
{
    if (!_voltageTopic.isEmpty()) {
        MqttSettings.unsubscribe(_voltageTopic);
    }

    if (!_socTopic.isEmpty()) {
        MqttSettings.unsubscribe(_socTopic);
    }

    _initialized = false;
}

void MqttBattery::onMqttMessageSoC(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto soc = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);

    if (!soc.has_value()) { return; }

    if (*soc < 0 || *soc > 100) {
        MessageOutput.printf("MqttBattery: Implausible SoC '%.2f' in topic '%s'\r\n",
                *soc, topic);
        return;
    }

    _stats->setSoC(*soc, 0/*precision*/, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated SoC to %d from '%s'\r\n",
                static_cast<uint8_t>(*soc), topic);
    }
}

void MqttBattery::onMqttMessageVoltage(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto voltage = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);

    if (!voltage.has_value()) { return; }

    auto const& config = Configuration.get();
    using Unit_t = BatteryVoltageUnit;
    switch (config.Battery.Mqtt.VoltageUnit) {
        case Unit_t::DeciVolts:
            *voltage /= 10;
            break;
        case Unit_t::CentiVolts:
            *voltage /= 100;
            break;
        case Unit_t::MilliVolts:
            *voltage /= 1000;
            break;
        default:
            break;
    }

    // since this project is revolving around Hoymiles microinverters, which can
    // only handle up to 65V of input voltage at best, it is safe to assume that
    // an even higher voltage is implausible.
    if (*voltage < 0 || *voltage > 65) {
        MessageOutput.printf("MqttBattery: Implausible voltage '%.2f' in topic '%s'\r\n",
                *voltage, topic);
        return;
    }

    _stats->setVoltage(*voltage, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated voltage to %.2f from '%s'\r\n",
                *voltage, topic);
    }
}
#endif
