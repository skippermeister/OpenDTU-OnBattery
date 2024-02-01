#ifdef USE_ModbusDTU

#pragma once

#include "Configuration.h"
#include <Hoymiles.h>
#include <ModbusIP_ESP8266.h>
#include <TaskSchedulerDeclarations.h>
#include <cstdint>

class ModbusDtuClass {
public:
    ModbusDtuClass();
    void init(Scheduler& scheduler);

private:
    void loop();
    void mbloop();
    void initModbus();

    void setHRegs(uint16_t reg, float value);
    void addHString(uint16_t reg, const char* s, uint8_t len);

    Task _loopTask;
    Task _mbloopTask;

    uint32_t _lastPublish = 0;
    bool _isstarted = false;
};

extern ModbusDtuClass ModbusDtu;

#endif
