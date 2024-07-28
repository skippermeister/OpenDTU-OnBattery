// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler, Malte Schmidt and others
 */
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "MqttHandlePowerLimiter.h"
#include "PowerLimiter.h"
#include <ctime>
#include <string>

static const char TAG[] = "[PowerLimiter MQTT]";

#define TOPIC_SUB_POWER_LIMITER "mode"

MqttHandlePowerLimiterClass MqttHandlePowerLimiter;

MqttHandlePowerLimiterClass::MqttHandlePowerLimiterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandlePowerLimiterClass::loop, this))
{
}

void MqttHandlePowerLimiterClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, MqttPowerLimiterCommand command) {
        String fullTopic(prefix + "powerlimiter/cmd/" + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
                std::bind(&MqttHandlePowerLimiterClass::onMqttCmd, this, command,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6));
    };

    subscribe("threshold/soc/start", MqttPowerLimiterCommand::BatterySoCStartThreshold);
    subscribe("threshold/soc/stop", MqttPowerLimiterCommand::BatterySoCStopThreshold);
    subscribe("threshold/soc/full_solar_passthrough", MqttPowerLimiterCommand::FullSolarPassthroughSoC);
    subscribe("threshold/voltage/start", MqttPowerLimiterCommand::VoltageStartThreshold);
    subscribe("threshold/voltage/stop", MqttPowerLimiterCommand::VoltageStopThreshold);
    subscribe("threshold/voltage/full_solar_passthrough_start", MqttPowerLimiterCommand::FullSolarPassThroughStartVoltage);
    subscribe("threshold/voltage/full_solar_passthrough_stop", MqttPowerLimiterCommand::FullSolarPassThroughStopVoltage);
    subscribe("mode", MqttPowerLimiterCommand::Mode);
    subscribe("upper_power_limit", MqttPowerLimiterCommand::UpperPowerLimit);
    subscribe("target_power_consumption", MqttPowerLimiterCommand::TargetPowerConsumption);

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);
}

void MqttHandlePowerLimiterClass::loop()
{
    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    const CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() || !_lastPublish.occured()) {
        return;
    }

    _lastPublish.set(Configuration.get().Mqtt.PublishInterval * 1000);

    auto val = static_cast<unsigned>(PowerLimiter.getMode());
    MqttSettings.publish("powerlimiter/status/mode", String(val));

    MqttSettings.publish("powerlimiter/status/upper_power_limit", String(config.PowerLimiter.UpperPowerLimit));

    MqttSettings.publish("powerlimiter/status/target_power_consumption", String(config.PowerLimiter.TargetPowerConsumption));

    MqttSettings.publish("powerlimiter/status/inverter_update_timeouts", String(PowerLimiter.getInverterUpdateTimeouts()));

    // no thresholds are relevant for setups without a battery
    if (config.PowerLimiter.IsInverterSolarPowered) { return; }

    MqttSettings.publish("powerlimiter/status/threshold/voltage/start", String(config.PowerLimiter.VoltageStartThreshold));
    MqttSettings.publish("powerlimiter/status/threshold/voltage/stop", String(config.PowerLimiter.VoltageStopThreshold));

    if (config.Vedirect.Enabled) {
        MqttSettings.publish("powerlimiter/status/full_solar_passthrough_active", String(PowerLimiter.getFullSolarPassThroughEnabled()));
        MqttSettings.publish("powerlimiter/status/threshold/voltage/full_solar_passthrough_start", String(config.PowerLimiter.FullSolarPassThroughStartVoltage));
        MqttSettings.publish("powerlimiter/status/threshold/voltage/full_solar_passthrough_stop", String(config.PowerLimiter.FullSolarPassThroughStopVoltage));
    }

    if (!config.Battery.Enabled || config.PowerLimiter.IgnoreSoc) { return; }

    MqttSettings.publish("powerlimiter/status/threshold/soc/start", String(config.PowerLimiter.BatterySocStartThreshold));
    MqttSettings.publish("powerlimiter/status/threshold/soc/stop", String(config.PowerLimiter.BatterySocStopThreshold));

    if (config.Vedirect.Enabled) {
        MqttSettings.publish("powerlimiter/status/threshold/soc/full_solar_passthrough", String(config.PowerLimiter.FullSolarPassThroughSoc));
    }
}

void MqttHandlePowerLimiterClass::onMqttCmd(MqttPowerLimiterCommand command, const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    CONFIG_T& config = Configuration.get();

    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    }
    catch (std::invalid_argument const& e) {
        MessageOutput.printf("PowerLimiter MQTT handler: cannot parse payload of topic '%s' as float: %s\r\n",
                topic, strValue.c_str());
        return;
    }
    const int intValue = static_cast<int>(payload_val);

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    switch (command) {
        case MqttPowerLimiterCommand::Mode:
            {
                using Mode = PowerLimiterClass::Mode;
                Mode mode = static_cast<Mode>(intValue);
                if (mode == Mode::UnconditionalFullSolarPassthrough) {
                    MessageOutput.println("Power limiter unconditional full solar PT");
                    _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                                &PowerLimiter, Mode::UnconditionalFullSolarPassthrough));
                } else if (mode == Mode::Disabled) {
                    MessageOutput.println("Power limiter disabled (override)");
                    _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                                &PowerLimiter, Mode::Disabled));
                } else if (mode == Mode::Normal) {
                    MessageOutput.println("Power limiter normal operation");
                    _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                                &PowerLimiter, Mode::Normal));
                } else {
                    MessageOutput.printf("PowerLimiter - unknown mode %d\r\n", intValue);
                }
                return;
            }
        case MqttPowerLimiterCommand::BatterySoCStartThreshold:
            if (config.PowerLimiter.BatterySocStartThreshold == intValue) { return; }
            MessageOutput.printf("Setting battery SoC start threshold to: %d %%\r\n", intValue);
            config.PowerLimiter.BatterySocStartThreshold = intValue;
            break;
        case MqttPowerLimiterCommand::BatterySoCStopThreshold:
            if (config.PowerLimiter.BatterySocStopThreshold == intValue) { return; }
            MessageOutput.printf("Setting battery SoC stop threshold to: %d %%\r\n", intValue);
            config.PowerLimiter.BatterySocStopThreshold = intValue;
            break;
        case MqttPowerLimiterCommand::FullSolarPassthroughSoC:
            if (config.PowerLimiter.FullSolarPassThroughSoc == intValue) { return; }
            MessageOutput.printf("Setting full solar passthrough SoC to: %d %%\r\n", intValue);
            config.PowerLimiter.FullSolarPassThroughSoc = intValue;
            break;
        case MqttPowerLimiterCommand::VoltageStartThreshold:
            if (config.PowerLimiter.VoltageStartThreshold == payload_val) { return; }
            MessageOutput.printf("Setting voltage start threshold to: %.2f V\r\n", payload_val);
            config.PowerLimiter.VoltageStartThreshold = payload_val;
            break;
        case MqttPowerLimiterCommand::VoltageStopThreshold:
            if (config.PowerLimiter.VoltageStopThreshold == payload_val) { return; }
            MessageOutput.printf("Setting voltage stop threshold to: %.2f V\r\n", payload_val);
            config.PowerLimiter.VoltageStopThreshold = payload_val;
            break;
        case MqttPowerLimiterCommand::FullSolarPassThroughStartVoltage:
            if (config.PowerLimiter.FullSolarPassThroughStartVoltage == payload_val) { return; }
            MessageOutput.printf("Setting full solar passthrough start voltage to: %.2f V\r\n", payload_val);
            config.PowerLimiter.FullSolarPassThroughStartVoltage = payload_val;
            break;
        case MqttPowerLimiterCommand::FullSolarPassThroughStopVoltage:
            if (config.PowerLimiter.FullSolarPassThroughStopVoltage == payload_val) { return; }
            MessageOutput.printf("Setting full solar passthrough stop voltage to: %.2f V\r\n", payload_val);
            config.PowerLimiter.FullSolarPassThroughStopVoltage = payload_val;
            break;
        case MqttPowerLimiterCommand::UpperPowerLimit:
            if (config.PowerLimiter.UpperPowerLimit == intValue) { return; }
            MessageOutput.printf("Setting upper power limit to: %d W\r\n", intValue);
            config.PowerLimiter.UpperPowerLimit = intValue;
            break;
        case MqttPowerLimiterCommand::TargetPowerConsumption:
            if (config.PowerLimiter.TargetPowerConsumption == intValue) { return; }
            MessageOutput.printf("Setting target power consumption to: %d W\r\n", intValue);
            config.PowerLimiter.TargetPowerConsumption = intValue;
            break;
    }

    // not reached if the value did not change
    Configuration.write();
}
