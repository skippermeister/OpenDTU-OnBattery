// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include "SDM.h"
#include "sml.h"
#include <Arduino.h>
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <espMqttClient.h>
#include <list>
#include <map>
#include <mutex>

#ifndef SDM_RX_PIN
#define SDM_RX_PIN 13
#endif

#ifndef SDM_TX_PIN
#define SDM_TX_PIN 32
#endif

#ifndef SML_RX_PIN
#define SML_RX_PIN 35
#endif

typedef struct {
    const unsigned char OBIS[6];
    void (*Fn)(double&);
    float* Arg;
} OBISHandler;

typedef struct {
    float Power1;
    float Power2;
    float Power3;
    float Voltage1;
    float Voltage2;
    float Voltage3;
    float Import;
    float Export;
    float PowerTotal;
    float HousePower;

} PowerMeter_t;

class PowerMeterClass {
public:
    enum SOURCE {
        SOURCE_MQTT = 0,
        SOURCE_SDM1PH = 1,
        SOURCE_SDM3PH = 2,
        SOURCE_HTTP = 3,
        SOURCE_SML = 4
    };
    PowerMeterClass();
    void init(Scheduler& scheduler);
    float getPowerTotal(bool forceUpdate = true);
    void setHousePower(float value);
    float getHousePower(void);
    uint32_t getLastPowerMeterUpdate(void);

    bool getVerboseLogging(void) { return _verboseLogging; };
    void setVerboseLogging(bool logging) { _verboseLogging = logging; };

private:
    void loop();

    Task _loopTask;

    void mqtt();
    void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total);

    TimeoutHelper _lastPowerMeterCheck;
    // Used in Power limiter for safety check
    uint32_t _lastPowerMeterUpdate;

    PowerMeter_t _powerMeter = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    PowerMeter_t _lastPowerMeter = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    std::map<String, float*> _mqttSubscriptions;

    mutable std::mutex _mutex;

    void readPowerMeter();

    bool smlReadLoop();
    const std::list<OBISHandler> smlHandlerList {
        { { 0x01, 0x00, 0x10, 0x07, 0x00, 0xff }, &smlOBISW, &_powerMeter.Power1 }
        //        {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeter.Import},
        //        {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeter.Export}
    };

    bool _verboseLogging = false;
};

extern PowerMeterClass PowerMeter;
