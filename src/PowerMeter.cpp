// SPDX-License-Identifier: GPL-2.0-or-later
#include "PowerMeter.h"
#include "Configuration.h"
#include "PowerMeterHttpJson.h"
#include "PowerMeterHttpSml.h"
#include "PowerMeterMqtt.h"
#include "PowerMeterSerialSdm.h"
#include "PowerMeterSerialSml.h"
#include "PowerMeterUdpSmaHomeManager.h"
#include "MessageOutput.h"

PowerMeterClass PowerMeter;

void PowerMeterClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&PowerMeterClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void PowerMeterClass::updateSettings()
{
    std::lock_guard<std::mutex> l(_mutex);

    if (_upProvider) { _upProvider.reset(); }

    auto const& pmcfg = Configuration.get().PowerMeter;

    if (!pmcfg.Enabled) { return; }

    switch(static_cast<PowerMeterProvider::Type>(pmcfg.Source)) {
        case PowerMeterProvider::Type::MQTT:
            _upProvider = std::make_unique<PowerMeterMqtt>(pmcfg.Mqtt);
            break;
        case PowerMeterProvider::Type::SDM1PH:
            _upProvider = std::make_unique<PowerMeterSerialSdm>(
                    PowerMeterSerialSdm::Phases::One, pmcfg.SerialSdm);
            break;
        case PowerMeterProvider::Type::SDM3PH:
            _upProvider = std::make_unique<PowerMeterSerialSdm>(
                    PowerMeterSerialSdm::Phases::Three, pmcfg.SerialSdm);
            break;
       case PowerMeterProvider::Type::HTTP_JSON:
            _upProvider = std::make_unique<PowerMeterHttpJson>(pmcfg.HttpJson);
            break;
        case PowerMeterProvider::Type::SERIAL_SML:
            _upProvider = std::make_unique<PowerMeterSerialSml>();
            break;
        case PowerMeterProvider::Type::SMAHM2:
            _upProvider = std::make_unique<PowerMeterUdpSmaHomeManager>();
            break;
        case PowerMeterProvider::Type::HTTP_SML:
            _upProvider = std::make_unique<PowerMeterHttpSml>(pmcfg.HttpSml);
            break;
    }

    if (!_upProvider->init()) { MessageOutput.println("Error powermeter provider"); _upProvider = nullptr; }
}

float PowerMeterClass::getHousePower() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0.0; }
    return _upProvider->getHousePower();
}

float PowerMeterClass::getPowerTotal() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0.0; }
    return _upProvider->getPowerTotal();
}

uint32_t PowerMeterClass::getLastUpdate() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0; }
    return _upProvider->getLastUpdate();
}

bool PowerMeterClass::isDataValid() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return false; }
    return _upProvider->isDataValid();
}

void PowerMeterClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_upProvider) { return; }
    _upProvider->loop();
    _upProvider->mqttLoop();
}
