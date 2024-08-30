// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Helge Erbe and others
 */
#ifdef USE_REFUsol_INVERTER

#include "MqttHandleREFUsol.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "REFUsolRS485Receiver.h"

MqttHandleREFUsolClass MqttHandleREFUsol;

MqttHandleREFUsolClass::MqttHandleREFUsolClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MqttHandleREFUsolClass::loop, this))
{
}

void MqttHandleREFUsolClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _lastPublish = millis();
}

#define MQTTpublish(value)                                                \
    if (!config.REFUsol.UpdatesOnly || _last.value != REFUsol.Frame.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = REFUsol.Frame.value));

void MqttHandleREFUsolClass::loop()
{
    auto const& config = Configuration.get();

    if (!config.REFUsol.Enabled) {
        return;
    }

    if (!MqttSettings.getConnected() ||
        (millis() - _lastPublish) < config.Mqtt.PublishInterval * 1000 ||
        !REFUsol.isDataValid())
    {
        return;
    }

    String value;
    String subtopic = "refusol/";
    subtopic.concat(REFUsol.Frame.serNo);
    subtopic.concat("/");

    if (!config.REFUsol.UpdatesOnly || strcmp(REFUsol.Frame.serNo, _last.serNo) != 0)
        MqttSettings.publish(subtopic + "serNo", REFUsol.Frame.serNo);
    if (!config.REFUsol.UpdatesOnly || strcmp(REFUsol.Frame.firmware, _last.firmware) != 0)
        MqttSettings.publish(subtopic + "firmware", REFUsol.Frame.firmware);
    MQTTpublish(currentStateOfOperation);
    MQTTpublish(error);
    MQTTpublish(status);
    MQTTpublish(totalOperatingHours);
    MQTTpublish(acVoltage);
    MQTTpublish(acVoltageL1);
    MQTTpublish(acVoltageL2);
    MQTTpublish(acVoltageL3);
    MQTTpublish(acCurrent);
    MQTTpublish(acCurrentL1);
    MQTTpublish(acCurrentL2);
    MQTTpublish(acCurrentL2);
    MQTTpublish(acPower);
    MQTTpublish(freqL1);
    MQTTpublish(freqL2);
    MQTTpublish(freqL3);
    MQTTpublish(dcPower);
    MQTTpublish(dcVoltage);
    MQTTpublish(dcCurrent);
    MQTTpublish(YieldDay);
    MQTTpublish(YieldMonth);
    MQTTpublish(YieldYear);
    MQTTpublish(YieldTotal);
    MQTTpublish(pvPeak);
    MQTTpublish(pvLimit);
    MQTTpublish(cosPhi);
    MQTTpublish(temperatureRight);
    MQTTpublish(temperatureTopLeft);
    MQTTpublish(temperatureBottomRight);
    MQTTpublish(temperatureLeft);

    _lastPublish = millis();
}

#endif
