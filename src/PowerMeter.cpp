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
#include "SerialPortManager.h"
#include <Arduino.h>
#include <ctime>
#include <SMA_HM.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr char TAG[] = "[PowerMeter]";

PowerMeterClass PowerMeter;

#ifndef USE_POWERMETER_HWSERIAL
  static SoftwareSerial sdmSerial;
#endif

PowerMeterClass::PowerMeterClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&PowerMeterClass::loop, this))
{
}

void PowerMeterClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Power Meter... ");

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _powerMeterValuesUpdated = 0;
    _powerMeterTimeUpdated = 0;
    _lastPowerMeterUpdate = 0;

    for (auto const& s : _mqttSubscriptions) { MqttSettings.unsubscribe(s.first); }
    _mqttSubscriptions.clear();

    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    if (!cPM.Enabled) {
        MessageOutput.println("disabled by configuration");
        return;
    }

    const PinMapping_t& pin = PinMapping.get();

    switch (static_cast<Source>(cPM.Source)) {
    case Source::MQTT: {
        MessageOutput.print("MQTT ");
        auto subscribe = [this](char const* topic, float* target) {
            if (strlen(topic) == 0) {
                MessageOutput.println("topic missing");
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
        }
        break;

    case Source::SDM1PH:
    case Source::SDM3PH:
        if (pin.powermeter_rx < 0 || pin.powermeter_tx < 0
            || pin.powermeter_rx == pin.powermeter_tx
            || (pin.powermeter_rts >= 0
                && (pin.powermeter_rts == pin.powermeter_rx || pin.powermeter_rts == pin.powermeter_tx)
               )
           )
        {
            MessageOutput.printf("invalid pin config for SDM power meter (RX = %d, TX = %d, RTS = %d)\r\n",
                pin.powermeter_rx, pin.powermeter_tx, pin.powermeter_rts);
            return;
        }

#if defined(USE_POWERMETER_HWSERIAL)
        auto oHwSerialPort = SerialPortManager.allocatePort(_sdmSerialPortOwner);
        if (!oHwSerialPort) { return; }

        _upSdmSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);
        _upSdmSerial->end(); // make sure the UART will be re-initialized
        _upSdm = std::make_unique<SDM>(*_upSdmSerial, 9600, pin.powermeter_rts, SERIAL_8N1, pin.powermeter_rx, pin.powermeter_tx);
        }
#else
//        else
        {
            MessageOutput.print("SWserial ");
            _upSdm = std::make_unique<SDM>(sdmSerial, 9600, pin.powermeter_rts, SWSERIAL_8N1, pin.powermeter_rx, pin.powermeter_tx);
        }
#endif
        MessageOutput.printf("RS485 (Type %d) port rx = %d, tx = %d", pin.powermeter_rts >= 0 ? 1 : 2, pin.powermeter_rx, pin.powermeter_tx);
        if (pin.powermeter_rts >= 0) MessageOutput.printf(", rts = %d", pin.powermeter_rts);
        MessageOutput.print(". ");
#if defined(USE_POWERMETER_HWSERIAL)
        _upSdm->begin(*oHwSerialPort);
#else
        _upSdm->begin();
#endif
        break;

    case Source::HTTP:
        HttpPowerMeter.init();
        break;

    case Source::SML:
        if (pin.powermeter_rx < 0 ||
            (pin.powermeter_tx >= 0 && (pin.powermeter_rx == pin.powermeter_tx))
           )
        {
            MessageOutput.printf("invalid pin config for SML power meter (RX = %d, TX = %d)\r\n", pin.powermeter_rx, pin.powermeter_tx);
            return;
        }

        MessageOutput.printf("SWserial SML rx = %d, tx = %d. ", pin.powermeter_rx, pin.powermeter_tx);

        pinMode(pin.powermeter_rx, INPUT);
        if (pin.powermeter_tx >= 0) pinMode(pin.powermeter_tx, OUTPUT);
        _upSmlSerial = std::make_unique<SoftwareSerial>();
        _upSmlSerial->begin(9600, SWSERIAL_8N1, pin.powermeter_rx, pin.powermeter_tx, false, 128, 95);
        _upSmlSerial->enableRx(true);
        _upSmlSerial->enableTx((pin.powermeter_tx >= 0) ? true : false);
        _upSmlSerial->flush();
        break;

#ifdef USE_SMA_HM
    case Source::SMAHM2:
        MessageOutput.print("SMA_HM ");
        SMA_HM.init(scheduler);
        break;
#endif
    }

#define POWER_METER_TASK_SIZE   8192

    BaseType_t task_created = xTaskCreatePinnedToCore(PowerMeter_task,
        "PowerMeter_task",
        POWER_METER_TASK_SIZE,
        this,
        +8, // POWER_METER_TASK_PRIORITY,
        &PowerMeter_task_handle,
        1);

    if (!task_created) {
        MessageOutput.println("error: creating PowerMeter Task");
        return;
    }


    MessageOutput.println("done");
}

void PowerMeterClass::PowerMeter_task(void* arg)
{

    while(1) {
        // do nothing if PowerMeter is not enabled
        while (!Configuration.get().PowerMeter.Enabled) {
            vTaskDelay(500);    // give 500ms time
        }

        // if Source == HTTP wait till Network is connected
        while ((   static_cast<Source>(Configuration.get().PowerMeter.Source) == Source::HTTP
#if defined (USE_SMA_HM)
                || static_cast<Source>(Configuration.get().PowerMeter.Source) == Source::SMAHM2
#endif
               )
               && !NetworkSettings.isConnected())
        {
            vTaskDelay(500);    // wait 500ms
        }

        uint32_t t_start = millis();

        PowerMeter.readPowerMeter();

        if (PowerMeter._verboseLogging) {
            MessageOutput.printf("%s%s Round trip %lu ms\r\n", TAG, "task", millis() - t_start);
        }

        vTaskDelay(Configuration.get().PowerMeter.PollInterval * 1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
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

        if (_verboseLogging) {
            MessageOutput.printf("%s Updated from '%s', TotalPower: %5.2f\r\n", TAG, topic, getPowerTotal(false));
        }

        _powerMeterTimeUpdated = millis();
        _powerMeterValuesUpdated++;
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
/*    if (forceUpdate) {
        if (Configuration.get().PowerMeter.Enabled
            && (millis() - _lastPowerMeterUpdate) > 1000) {
            readPowerMeter();
        }
    }
*/
    std::lock_guard<std::mutex> l(_mutex);
    return _powerMeter.Power1 + _powerMeter.Power2 + _powerMeter.Power3;
}

uint32_t PowerMeterClass::getLastPowerMeterUpdate()
{
    std::lock_guard<std::mutex> l(_mutex);
    return _lastPowerMeterUpdate;
}

bool PowerMeterClass::isDataValid()
{
    auto const& config = Configuration.get();

    std::lock_guard<std::mutex> l(_mutex);

    bool valid = config.PowerMeter.Enabled &&
        _lastPowerMeterUpdate > 0 &&
        ((millis() - _lastPowerMeterUpdate) < (30 * 1000));

    // reset if timed out to avoid glitch once
    // (millis() - _lastPowerMeterUpdate) overflows
    if (!valid) { _lastPowerMeterUpdate = 0; }

    return valid;
}

void PowerMeterClass::mqtt()
{
    if (!MqttSettings.getConnected()) { return; }

    const PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    String topic = "powermeter/";
    auto PowerTotal = getPowerTotal(false);

    std::lock_guard<std::mutex> l(_mutex);
    if (!cPM.UpdatesOnly || _powerMeter.Power1 != _lastPowerMeter.Power1)
        MqttSettings.publish(topic + "power1", String(_lastPowerMeter.Power1 = _powerMeter.Power1));
    if (!cPM.UpdatesOnly || _powerMeter.Power2 != _lastPowerMeter.Power2)
        MqttSettings.publish(topic + "power2", String(_lastPowerMeter.Power2 = _powerMeter.Power2));
    if (!cPM.UpdatesOnly || _powerMeter.Power3 != _lastPowerMeter.Power3)
        MqttSettings.publish(topic + "power3", String(_lastPowerMeter.Power3 = _powerMeter.Power3));
    if (!cPM.UpdatesOnly || PowerTotal != _lastPowerMeter.PowerTotal)
        MqttSettings.publish(topic + "powertotal", String(_lastPowerMeter.PowerTotal = PowerTotal));
    if (!cPM.UpdatesOnly || getHousePower() != _lastPowerMeter.HousePower)
        MqttSettings.publish(topic + "housepower", String(_lastPowerMeter.HousePower = getHousePower()));

    if (static_cast<Source>(cPM.Source) != Source::HTTP) {
        if (!cPM.UpdatesOnly || _powerMeter.Voltage1 != _lastPowerMeter.Voltage1)
            MqttSettings.publish(topic + "voltage1", String(_lastPowerMeter.Voltage1 = _powerMeter.Voltage1));
        if (!cPM.UpdatesOnly || _powerMeter.Voltage2 != _lastPowerMeter.Voltage2)
            MqttSettings.publish(topic + "voltage2", String(_lastPowerMeter.Voltage2 = _powerMeter.Voltage2));
        if (!cPM.UpdatesOnly || _powerMeter.Voltage3 != _lastPowerMeter.Voltage3)
            MqttSettings.publish(topic + "voltage3", String(_lastPowerMeter.Voltage3 = _powerMeter.Voltage3));
        if (!cPM.UpdatesOnly || _powerMeter.Import != _lastPowerMeter.Import)
            MqttSettings.publish(topic + "import", String(_lastPowerMeter.Import = _powerMeter.Import));
        if (!cPM.UpdatesOnly || _powerMeter.Export != _lastPowerMeter.Export)
            MqttSettings.publish(topic + "export", String(_lastPowerMeter.Export = _powerMeter.Export));
    }
}

void PowerMeterClass::loop()
{
    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    if (!cPM.Enabled) { return; }

    if (static_cast<Source>(cPM.Source) == Source::SML &&
            nullptr != _upSmlSerial) {
        if (!smlReadLoop()) { return; }
    }

    if (static_cast<Source>(cPM.Source) == Source::HTTP && !NetworkSettings.isConnected()) { return; }

    // wait till PowerMeter Task, or onMqttMessage or SML reader indicate that the values has been updated
    if (!_powerMeterValuesUpdated) { return; }
    _powerMeterValuesUpdated--;

    _lastPowerMeterUpdate = millis();

    MessageOutput.printf("%s got new values, Tasks sync delay: %lu ms\r\n", TAG, millis()-_powerMeterTimeUpdated);

    float PowerTotal = getPowerTotal(false);
    PowerMeter.setHousePower(PowerTotal + Datastore.getTotalAcPowerEnabled());

    if (_verboseLogging) MessageOutput.printf("%s TotalPower: %5.1fW, HousePower: %5.1fW\r\n", TAG, PowerTotal, getHousePower());

    mqtt();
}

void PowerMeterClass::readPowerMeter()
{
    PowerMeter_CONFIG_T& cPM = Configuration.get().PowerMeter;

    uint8_t _address = cPM.Sdm.Address;
    Source configuredSource = static_cast<Source>(cPM.Source);

    if (configuredSource == Source::SDM1PH) {
        if (!_upSdm) { return; }

        // this takes a "very long" time as each readVal() is a synchronous
        // exchange of serial messages. cache the values and write later.
        auto phase1Power   = _upSdm->readVal(SDM_PHASE_1_POWER, _address, _verboseLogging);

        /*
         * other interesting values

            SDM_TOTAL_SYSTEM_POWER
            SDM_TOTAL_SYSTEM_POWER_FACTOR
            SDM_FREQUENCY
            SDM_PHASE_1_ANGLE
            SDM_IMPORT_POWER
            SDM_EXPORT_POWER
        */

        static uint8_t state = 0;
        switch (state) {
            case 0:
                {
                auto phase1Voltage = _upSdm->readVal(SDM_PHASE_1_VOLTAGE, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Voltage1 = static_cast<float>(phase1Voltage);
                _powerMeter.Voltage2 = 0;
                _powerMeter.Voltage3 = 0;
                state++;
                }
                break;
            case 1:
                {
                auto energyImport  = _upSdm->readVal(SDM_IMPORT_ACTIVE_ENERGY, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Import   = static_cast<float>(energyImport);
                state++;
                }
                break;
            case 2:
            default:
                {
                auto energyExport  = _upSdm->readVal(SDM_EXPORT_ACTIVE_ENERGY, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Export   = static_cast<float>(energyExport);
                state = 0;
                }
        }

        _powerMeter.Power1 = static_cast<float>(phase1Power);
        _powerMeter.Power2 = 0;
        _powerMeter.Power3 = 0;

        _powerMeterTimeUpdated = millis();  // timestamp of update
        _powerMeterValuesUpdated++; // indicate we updated

    } else if (configuredSource == Source::SDM3PH) {
        if (!_upSdm) { return; }

        // this takes a "very long" time as each readVal() is a synchronous
        // exchange of serial messages. cache the values and write later.
        auto phase1Power   = _upSdm->readVal(SDM_PHASE_1_POWER, _address, _verboseLogging);
        auto phase2Power   = _upSdm->readVal(SDM_PHASE_2_POWER, _address, _verboseLogging);
        auto phase3Power   = _upSdm->readVal(SDM_PHASE_3_POWER, _address, _verboseLogging);

        /*
         * other interesting vales

            SDM_TOTAL_SYSTEM_POWER
            SDM_TOTAL_SYSTEM_POWER_FACTOR
            SDM_FREQUENCY
            SDM_PHASE_1_ANGLE
            SDM_PHASE_2_ANGLE
            SDM_PHASE_3_ANGLE
            SDM_IMPORT_POWER
            SDM_EXPORT_POWER
        */

        static uint8_t state = 0;
        switch (state) {
            case 0:
                {
                auto phase1Voltage = _upSdm->readVal(SDM_PHASE_1_VOLTAGE, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Voltage1 = static_cast<float>(phase1Voltage);
                state++;
                }
                break;
            case 1:
                {
                auto phase2Voltage = _upSdm->readVal(SDM_PHASE_2_VOLTAGE, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Voltage2 = static_cast<float>(phase2Voltage);
                state++;
                }
                break;
            case 2:
                {
                auto phase3Voltage = _upSdm->readVal(SDM_PHASE_3_VOLTAGE, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Voltage3 = static_cast<float>(phase3Voltage);
                state++;
                }
                break;
            case 3:
                {
                auto energyImport  = _upSdm->readVal(SDM_IMPORT_ACTIVE_ENERGY, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Import   = static_cast<float>(energyImport);
                state++;
                }
                break;
            case 4:
            default:;
                {
                auto energyExport  = _upSdm->readVal(SDM_EXPORT_ACTIVE_ENERGY, _address, _verboseLogging);
                std::lock_guard<std::mutex> l(_mutex);
                _powerMeter.Export   = static_cast<float>(energyExport);
                state=0;
                }
        }

        _powerMeter.Power1   = static_cast<float>(phase1Power);
        _powerMeter.Power2   = static_cast<float>(phase2Power);
        _powerMeter.Power3   = static_cast<float>(phase3Power);

        _powerMeterTimeUpdated = millis();  // timestamp of update
        _powerMeterValuesUpdated++; // indicate we updated

    } else if (configuredSource == Source::HTTP) {
        if (HttpPowerMeter.updateValues()) {
            std::lock_guard<std::mutex> l(_mutex);
            _powerMeter.Power1 = HttpPowerMeter.getPower(1);
            _powerMeter.Power2 = HttpPowerMeter.getPower(2);
            _powerMeter.Power3 = HttpPowerMeter.getPower(3);

            _powerMeterTimeUpdated = millis();  // timestamp of update
            _powerMeterValuesUpdated++; // indicate we updated
        }
    }
#ifdef USE_SMA_HM
    else if (configuredSource == Source::SMAHM2) {
        std::lock_guard<std::mutex> l(_mutex);
        _powerMeter.Power1 = SMA_HM.getPowerL1();
        _powerMeter.Power2 = SMA_HM.getPowerL2();
        _powerMeter.Power3 = SMA_HM.getPowerL3();

        _powerMeterTimeUpdated = millis();  // timestamp of update
        _powerMeterValuesUpdated++; // indicate we updated
    }
#endif
}

bool PowerMeterClass::smlReadLoop()
{
    while (_upSmlSerial->available()) {
        double readVal = 0;
        unsigned char smlCurrentChar = _upSmlSerial->read();
        sml_states_t smlCurrentState = smlState(smlCurrentChar);
        if (smlCurrentState == SML_LISTEND) {
            for (auto& handler : smlHandlerList) {
                if (smlOBISCheck(handler.OBIS)) {
                    handler.Fn(readVal);
                    *handler.Arg = readVal;
                }
            }
        } else if (smlCurrentState == SML_FINAL) {
            _powerMeterTimeUpdated = millis();  // timestamp of update
            _powerMeterValuesUpdated++; // indicate we updated
            return true;
        }
    }

    return false;
}
