#ifdef USE_ModbusDTU

#pragma once

#include <TaskSchedulerDeclarations.h>
#include <cstdint>
#include "Configuration.h"
#include <ModbusIP_ESP8266.h>
#include <Hoymiles.h>

class ModbusDtuClass {
public:
    ModbusDtuClass();
    void init(Scheduler& scheduler);

private:
    void loop();
    void setup();
    void modbus();

    void setHRegs(uint16_t reg, float value);
    void addHString(uint16_t reg, const char* s, uint8_t len);

    bool _isstarted = false;
    float _lasttotal = 0;

    Task _loopTask;
    Task _modbusTask;
};

extern ModbusDtuClass ModbusDtu;

#endif
