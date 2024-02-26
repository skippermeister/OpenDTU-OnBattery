// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "VeDirectMpptController.h"
#include "Configuration.h"
#include <Arduino.h>
#include <TaskSchedulerDeclarations.h>

class MqttHandleVedirectClass {
public:
    MqttHandleVedirectClass();
    void init(Scheduler& scheduler);
    void forceUpdate();

private:
    void loop();
    VeDirectMpptController::veMpptStruct _kvFrame{};

    Task _loopTask;

    // point of time in millis() when updated values will be published
    uint32_t _nextPublishUpdatesOnly = 0;

    // point of time in millis() when all values will be published
    uint32_t _nextPublishFull = 1;

    bool _PublishFull;
};

extern MqttHandleVedirectClass MqttHandleVedirect;
