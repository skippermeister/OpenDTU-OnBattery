// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "PowerMeter.h"
#include "Configuration.h"
#include "Datastore.h"
#include "HttpPowerMeter.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "SDM.h"
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ctime>

static char TAG[] = "[PowerMeter]";

PowerMeterClass PowerMeter;

#ifndef USE_HARDWARESERIAL
EspSoftwareSerial::UART sdmSerial;
SDM sdm(sdmSerial, 9600, DERE_PIN, EspSoftwareSerial::SWSERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);
#else
SDM sdm(Serial2, 9600, DERE_PIN, SERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);
#endif

EspSoftwareSerial::UART inputSerial;

PowerMeterClass::PowerMeterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&PowerMeterClass::loop, this))
{
}

void PowerMeterClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Power Meter... ");

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _lastPowerMeterCheck.set(Configuration.get().PowerMeter.PollInterval * 1000);
    _lastPowerMeterUpdate = 0;

    for (auto const& s : _mqttSubscriptions) {
        MqttSettings.unsubscribe(s.first);
    }
    _mqttSubscriptions.clear();

    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    if (!cPM.Enabled) {
        MessageOutput.println("disabled by configuration");
        return;
    }

    switch (cPM.Source) {
    case SOURCE_MQTT: {
        auto subscribe = [this](char const* topic, float* target) {
            if (strlen(topic) == 0) {
                return;
            }
            MqttSettings.subscribe(topic, 0,
                std::bind(&PowerMeterClass::onMqttMessage,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6));
            _mqttSubscriptions.try_emplace(topic, target);
        };
        subscribe(cPM.Mqtt.TopicPowerMeter1, &_powerMeter.Power1);
        subscribe(cPM.Mqtt.TopicPowerMeter2, &_powerMeter.Power2);
        subscribe(cPM.Mqtt.TopicPowerMeter3, &_powerMeter.Power3);
    } break;

    case SOURCE_SDM1PH:
    case SOURCE_SDM3PH:
        sdm.begin();
        break;

    case SOURCE_HTTP:
        HttpPowerMeter.init();
        break;

    case SOURCE_SML:
        pinMode(SML_RX_PIN, INPUT);
        inputSerial.begin(9600, EspSoftwareSerial::SWSERIAL_8N1, SML_RX_PIN, -1, false, 128, 95);
        inputSerial.enableRx(true);
        inputSerial.enableTx(false);
        inputSerial.flush();
        break;
    }

    MessageOutput.println("done");
}

void PowerMeterClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    for (auto const& subscription : _mqttSubscriptions) {
        if (subscription.first != topic) {
            continue;
        }

        std::string value(reinterpret_cast<const char*>(payload), len);
        try {
            *subscription.second = std::stof(value);
        } catch (std::invalid_argument const& e) {
            MessageOutput.printf("%s cannot parse payload of topic '%s' as float: %s\r\n", TAG,
                topic, value.c_str());
            return;
        }

        if (_verbose_logging) {
            MessageOutput.printf("%s Updated from '%s', TotalPower: %5.2f\r\n", TAG, topic, getPowerTotal(false));
        }

        _lastPowerMeterUpdate = millis();
    }
}

void PowerMeterClass::setHousePower(float value)
{
    _powerMeter.HousePower = value;
}

float PowerMeterClass::getHousePower(void)
{
    return _powerMeter.HousePower;
}

float PowerMeterClass::getPowerTotal(bool forceUpdate)
{
    if (forceUpdate) {
        if (Configuration.get().PowerMeter.Enabled
            && (millis() - _lastPowerMeterUpdate) > 1000) {
            readPowerMeter();
        }
    }
    return _powerMeter.Power1 + _powerMeter.Power2 + _powerMeter.Power3;
}

void PowerMeterClass::mqtt()
{
    if (!MqttSettings.getConnected()) {
        return;
    } else {

        const PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

        String topic = "powermeter/";
        if (!cPM.UpdatesOnly || _powerMeter.Power1 != _lastPowerMeter.Power1)
            MqttSettings.publish(topic + "power1", String(_lastPowerMeter.Power1 = _powerMeter.Power1));
        if (!cPM.UpdatesOnly || _powerMeter.Power2 != _lastPowerMeter.Power2)
            MqttSettings.publish(topic + "power2", String(_lastPowerMeter.Power2 = _powerMeter.Power2));
        if (!cPM.UpdatesOnly || _powerMeter.Power3 != _lastPowerMeter.Power3)
            MqttSettings.publish(topic + "power3", String(_lastPowerMeter.Power3 = _powerMeter.Power3));
        float PowerTotal = getPowerTotal(false);
        if (!cPM.UpdatesOnly || PowerTotal != _lastPowerMeter.PowerTotal)
            MqttSettings.publish(topic + "powertotal", String(_lastPowerMeter.PowerTotal = PowerTotal));
        if (!cPM.UpdatesOnly || getHousePower() != _lastPowerMeter.HousePower)
            MqttSettings.publish(topic + "housepower", String(_lastPowerMeter.HousePower = getHousePower()));
        /*        if (!cpPM.UpdatesOnly || _powerMeter.Voltage1 != _lastPowerMeter.Voltage1)
                    MqttSettings.publish(topic + "voltage1", String(_lastPowerMeter.Voltage1 = _powerMeter.Voltage1));
                if (!cPM.UpdatesOnly || _powerMeter.Voltage2 != _lastPowerMeter.Voltage2)
                    MqttSettings.publish(topic + "voltage2", String(_lastPowerMeter.Voltage2 = _powerMeter.Voltage2));
                if (!cPM.UpdatesOnly || _powerMeter.Voltage3 != _lastPowerMeter.Voltage3)
                    MqttSettings.publish(topic + "voltage3", String(_lastPowerMeter.Voltage3 = _powerMeter.Voltage3));
                if (!cPM.UpdatesOnly || _powerMeter.Import != _lastPowerMeter.Import)
                    MqttSettings.publish(topic + "import", String(_lastPowerMeter.Import = _powerMeter.Import));
                if (!cPM.UpdatesOnly || _powerMeter.Export != _lastPowerMeter.Export)
                    MqttSettings.publish(topic + "export", String(_lastPowerMeter.Export = _powerMeter.Export));
        */
    }
}

void PowerMeterClass::loop()
{
    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    uint32_t t_start = millis();

    if (!cPM.Enabled) {
        return;
    }

    if (cPM.Source == SOURCE_SML) {
        if (!smlReadLoop()) {
            return;
        } else {
            _lastPowerMeterUpdate = millis();
        }
    }

    if (!NetworkSettings.isConnected()) {
        return;
    }

    if (!_lastPowerMeterCheck.occured()) {
        return;
    }

    readPowerMeter();

    float PowerTotal = getPowerTotal(false);
    PowerMeter.setHousePower(PowerTotal + Datastore.getTotalAcPowerEnabled());

    if (_verbose_logging)
        MessageOutput.printf("%s TotalPower: %5.1fW, HousePower: %5.1fW\r\n", TAG, PowerTotal, getHousePower());

    mqtt();

    _lastPowerMeterCheck.set(Configuration.get().PowerMeter.PollInterval * 1000);

    MessageOutput.printf("%s Round trip %lu ms\r\n", TAG, millis() - t_start);
}

void PowerMeterClass::readPowerMeter()
{
    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    uint8_t _address = cPM.Sdm.Address;

    if (cPM.Source == SOURCE_SDM1PH) {
        _powerMeter.Power1 = static_cast<float>(sdm.readVal(SDM_PHASE_1_POWER, _address));
        _powerMeter.Power2 = 0.0;
        _powerMeter.Power3 = 0.0;
        /*
                _powerMeter.Voltage1 = static_cast<float>(sdm.readVal(SDM_PHASE_1_VOLTAGE, _address));
                _powerMeter.Voltage2 = 0.0;
                _powerMeter.Voltage3 = 0.0;
                _powerMeter.Import = static_cast<float>(sdm.readVal(SDM_IMPORT_ACTIVE_ENERGY, _address));
                _powerMeter.Export = static_cast<float>(sdm.readVal(SDM_EXPORT_ACTIVE_ENERGY, _address));
        */
        _lastPowerMeterUpdate = millis();
    } else if (cPM.Source == SOURCE_SDM3PH) {
        _powerMeter.Power1 = static_cast<float>(sdm.readVal(SDM_PHASE_1_POWER, _address));
        _powerMeter.Power2 = static_cast<float>(sdm.readVal(SDM_PHASE_2_POWER, _address));
        _powerMeter.Power3 = static_cast<float>(sdm.readVal(SDM_PHASE_3_POWER, _address));
        /*
                _powerMeter.Voltage1 = static_cast<float>(sdm.readVal(SDM_PHASE_1_VOLTAGE, _address));
                _powerMeter.Voltage2 = static_cast<float>(sdm.readVal(SDM_PHASE_2_VOLTAGE, _address));
                _powerMeter.Voltage3 = static_cast<float>(sdm.readVal(SDM_PHASE_3_VOLTAGE, _address));
                _powerMeter.Import = static_cast<float>(sdm.readVal(SDM_IMPORT_ACTIVE_ENERGY, _address));
                _powerMeter.Export = static_cast<float>(sdm.readVal(SDM_EXPORT_ACTIVE_ENERGY, _address));
        */
        _lastPowerMeterUpdate = millis();
    } else if (cPM.Source == SOURCE_HTTP) {
        if (HttpPowerMeter.updateValues()) {
            _powerMeter.Power1 = HttpPowerMeter.getPower(1);
            _powerMeter.Power2 = HttpPowerMeter.getPower(2);
            _powerMeter.Power3 = HttpPowerMeter.getPower(3);
            /*
                        _powerMeter.Voltage1 = HttpPowerMeter.getVoltage(1);
                        _powerMeter.Voltage2 = HttpPowerMeter.getVoltage(2);
                        _powerMeter.Voltage3 = HttpPowerMeter.getVoltage(3);
            */
            /*            _powerMeter.Import = HttpPowerMeter.getTotalImport();
                        _powerMeter.Export = HttpPowerMeter.getTotalExport();
            */
            _lastPowerMeterUpdate = millis();
        }
    }
}

bool PowerMeterClass::smlReadLoop()
{
    while (inputSerial.available()) {
        double readVal = 0;
        unsigned char smlCurrentChar = inputSerial.read();
        sml_states_t smlCurrentState = smlState(smlCurrentChar);
        if (smlCurrentState == SML_LISTEND) {
            for (auto& handler : smlHandlerList) {
                if (smlOBISCheck(handler.OBIS)) {
                    handler.Fn(readVal);
                    *handler.Arg = readVal;
                }
            }
        } else if (smlCurrentState == SML_FINAL) {
            return true;
        }
    }

    return false;
}
