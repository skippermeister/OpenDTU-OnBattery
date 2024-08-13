// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_REFUsol_INVERTER

#include "Configuration.h"
#include "REFUsolRS485Receiver.h"
#include <Arduino.h>
#include <TaskSchedulerDeclarations.h>
#include <mutex>

#ifndef REFUsol_PIN_RX
#define REFUsol_PIN_RX 22
#endif

#ifndef REFUsol_PIN_TX
#define REFUsol_PIN_TX 21
#endif

#ifndef REFUsol_PIN_CTS
#define REFUsol_PIN_CTS -1
#endif

#ifndef REFUsol_PIN_RTS
#define REFUsol_PIN_RTS 21
#endif

class MqttHandleREFUsolClass {
public:
    MqttHandleREFUsolClass();
    void init(Scheduler& scheduler);

private:
    void loop();

    Task _loopTask;

    REFUsolStruct _last {}; // last value store for MQTT publishing

    uint32_t _lastPublish;

    // MQTT callbacks to process updates on subscribed topics are executed in
    // the MQTT thread's context. we use this queue to switch processing the
    // user requests into the main loop's context (TaskScheduler context).
    mutable std::mutex _mqttMutex;
    std::deque<std::function<void()>> _mqttCallbacks;
};

extern MqttHandleREFUsolClass MqttHandleREFUsol;

#endif
