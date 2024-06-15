// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Helge Erbe and others
 */
#include "MqttHandleVedirect.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "VictronMppt.h"

MqttHandleVedirectClass MqttHandleVedirect;

// #define MQTTHANDLEVEDIRECT_DEBUG
MqttHandleVedirectClass::MqttHandleVedirectClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleVedirectClass::loop, this))
{
}

void MqttHandleVedirectClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    // initially force a full publish
    this->forceUpdate();
}

void MqttHandleVedirectClass::forceUpdate()
{
    // initially force a full publish
    _nextPublishUpdatesOnly = 0;
    _nextPublishFull = 1;
}

void MqttHandleVedirectClass::loop()
{
    CONFIG_T& config = Configuration.get();

    if (!MqttSettings.getConnected() || !config.Vedirect.Enabled || !VictronMppt.isDataValid()) {
        return;
    }

    if ((millis() - _nextPublishFull >= ((config.Mqtt.PublishInterval * 3) - 1) * 1000)
        || (millis() - _nextPublishUpdatesOnly >= (config.Mqtt.PublishInterval * 1000))) {
        // determine if this cycle should publish full values or updates only
        if (_nextPublishFull + ((config.Mqtt.PublishInterval * 3) - 1) * 1000 <= _nextPublishUpdatesOnly + config.Mqtt.PublishInterval * 1000) {
            _PublishFull = true;
        } else {
            _PublishFull = !config.Vedirect.UpdatesOnly;
        }

#ifdef MQTTHANDLEVEDIRECT_DEBUG
        MessageOutput.printf("\r\n\r\nMqttHandleVedirectClass::loop millis %lu   _nextPublishUpdatesOnly %u   _nextPublishFull %u\r\n", millis(), _nextPublishUpdatesOnly, _nextPublishFull);
        if (_PublishFull) {
            MessageOutput.println("MqttHandleVedirectClass::loop publish full");
        } else {
            MessageOutput.println("MqttHandleVedirectClass::loop publish updates only");
        }
#endif

        for (int idx = 0; idx < VictronMppt.controllerAmount(); ++idx) {
            std::optional<VeDirectMpptController::data_t> optMpptData = VictronMppt.getData(idx);
            if (!optMpptData.has_value()) { continue; }

            auto const& kvFrame = _kvFrames[optMpptData->serialNr_SER];
            publish_mppt_data(*optMpptData, kvFrame);
            if (!_PublishFull) {
                _kvFrames[optMpptData->serialNr_SER] = *optMpptData;
            }
        }

        // now calculate next points of time to publish
        _nextPublishUpdatesOnly = millis() + (config.Mqtt.PublishInterval * 1000);

        if (_PublishFull) {
#ifdef USE_HASS
            // when Home Assistant MQTT-Auto-Discovery is active,
            // and "enable expiration" is active, all values must be published at
            // least once before the announced expiry interval is reached
            if ((config.Vedirect.UpdatesOnly) && (config.Mqtt.Hass.Enabled) && (config.Mqtt.Hass.Expire)) {
                _nextPublishFull = millis() + (((config.Mqtt.PublishInterval * 3) - 1) * 1000);

                #ifdef MQTTHANDLEVEDIRECT_DEBUG
                uint32_t _tmpNextFullSeconds = (config.Mqtt_PublishInterval * 3) - 1;
                MessageOutput.printf("MqttHandleVedirectClass::loop _tmpNextFullSeconds %u - _nextPublishFull %u \r\n", _tmpNextFullSeconds, _nextPublishFull);
                #endif

            } else
#endif
            {
                // no future publish full needed
                _nextPublishFull = UINT32_MAX;
            }
        }

        #ifdef MQTTHANDLEVEDIRECT_DEBUG
        MessageOutput.printf("MqttHandleVedirectClass::loop _nextPublishUpdatesOnly %u   _nextPublishFull %u\r\n", _nextPublishUpdatesOnly, _nextPublishFull);
        #endif
    }
}

void MqttHandleVedirectClass::publish_mppt_data(const VeDirectMpptController::data_t &currentData,
                                                const VeDirectMpptController::data_t &previousData) const {
    String value;
    String topic = "victron/";
    topic.concat(currentData.serialNr_SER);
    topic.concat("/");

    if (_PublishFull || currentData.productID_PID != previousData.productID_PID)
        MqttSettings.publish(topic + "PID", currentData.getPidAsString().data());
    if (_PublishFull || strcmp(currentData.serialNr_SER, previousData.serialNr_SER) != 0)
        MqttSettings.publish(topic + "SER", currentData.serialNr_SER);
    if (_PublishFull || strcmp(currentData.firmwareVer_FW, previousData.firmwareVer_FW) != 0)
        MqttSettings.publish(topic + "FW", currentData.firmwareVer_FW);
    if (_PublishFull || currentData.loadCurrent_IL_mA != previousData.loadCurrent_IL_mA)
        MqttSettings.publish(topic + "I", String(currentData.loadCurrent_IL_mA/1000.0));
    if (_PublishFull || currentData.loadOutputState_LOAD != previousData.loadOutputState_LOAD)
        MqttSettings.publish(topic + "LOAD", currentData.loadOutputState_LOAD == true ? "ON" : "OFF");
    if (_PublishFull || currentData.currentState_CS != previousData.currentState_CS)
        MqttSettings.publish(topic + "CS", currentData.getCsAsString().data());
    if (_PublishFull || currentData.errorCode_ERR != previousData.errorCode_ERR)
        MqttSettings.publish(topic + "ERR", currentData.getErrAsString().data());
    if (_PublishFull || currentData.offReason_OR != previousData.offReason_OR)
        MqttSettings.publish(topic + "OR", currentData.getOrAsString().data());
    if (_PublishFull || currentData.stateOfTracker_MPPT != previousData.stateOfTracker_MPPT)
        MqttSettings.publish(topic + "MPPT", currentData.getMpptAsString().data());
    if (_PublishFull || currentData.daySequenceNr_HSDS != previousData.daySequenceNr_HSDS)
        MqttSettings.publish(topic + "HSDS", String(currentData.daySequenceNr_HSDS));
    if (_PublishFull || currentData.batteryVoltage_V_mV != previousData.batteryVoltage_V_mV)
        MqttSettings.publish(topic + "V", String(currentData.batteryVoltage_V_mV/1000.0));
    if (_PublishFull || currentData.batteryCurrent_I_mA != previousData.batteryCurrent_I_mA)
        MqttSettings.publish(topic + "I", String(currentData.batteryCurrent_I_mA/1000.0));
    if (_PublishFull || currentData.batteryOutputPower_W != previousData.batteryOutputPower_W)
        MqttSettings.publish(topic + "P", String(currentData.batteryOutputPower_W));
    if (_PublishFull || currentData.panelVoltage_VPV_mV != previousData.panelVoltage_VPV_mV)
        MqttSettings.publish(topic + "VPV", String(currentData.panelVoltage_VPV_mV/1000.0));
    if (_PublishFull || currentData.panelCurrent_mA != previousData.panelCurrent_mA)
        MqttSettings.publish(topic + "IPV", String(currentData.panelCurrent_mA/1000.0));
    if (_PublishFull || currentData.panelPower_PPV_W != previousData.panelPower_PPV_W)
        MqttSettings.publish(topic + "PPV", String(currentData.panelPower_PPV_W));
    if (_PublishFull || currentData.mpptEfficiency_Percent != previousData.mpptEfficiency_Percent)
        MqttSettings.publish(topic + "E", String(currentData.mpptEfficiency_Percent));
    if (_PublishFull || currentData.yieldTotal_H19_Wh != previousData.yieldTotal_H19_Wh)
        MqttSettings.publish(topic + "H19", String(currentData.yieldTotal_H19_Wh/1000.0));
    if (_PublishFull || currentData.yieldToday_H20_Wh != previousData.yieldToday_H20_Wh)
        MqttSettings.publish(topic + "H20", String(currentData.yieldToday_H20_Wh/1000.0));
    if (_PublishFull || currentData.maxPowerToday_H21_W != previousData.maxPowerToday_H21_W)
        MqttSettings.publish(topic + "H21", String(currentData.maxPowerToday_H21_W));
    if (_PublishFull || currentData.yieldYesterday_H22_Wh != previousData.yieldYesterday_H22_Wh)
        MqttSettings.publish(topic + "H22", String(currentData.yieldYesterday_H22_Wh/1000.0));
    if (_PublishFull || currentData.maxPowerYesterday_H23_W != previousData.maxPowerYesterday_H23_W)
        MqttSettings.publish(topic + "H23", String(currentData.maxPowerYesterday_H23_W));
}
