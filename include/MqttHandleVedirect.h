// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "VeDirectMpptController.h"
#include "Configuration.h"
#include <Arduino.h>
#include <map>
#include <TaskSchedulerDeclarations.h>

class MqttHandleVedirectClass {
public:
    MqttHandleVedirectClass();
    void init(Scheduler& scheduler);
    void forceUpdate();

private:
    void loop();
    std::map<std::string, VeDirectMpptController::veMpptStruct> _kvFrames;

    Task _loopTask;

    // point of time in millis() when updated values will be published
    uint32_t _nextPublishUpdatesOnly = 0;

    // point of time in millis() when all values will be published
    uint32_t _nextPublishFull = 1;

    bool _PublishFull;

    void publish_mppt_data(const VeDirectMpptController::spData_t &spMpptData,
                           VeDirectMpptController::veMpptStruct &frame) const;
};

extern MqttHandleVedirectClass MqttHandleVedirect;
