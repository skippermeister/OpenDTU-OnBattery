// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifndef CHARGER_HUAWEI

#include <TaskSchedulerDeclarations.h>
#include <cstdint>
#ifdef CHARGER_USE_CAN0
#include <driver/twai.h>
#else
#include "SPI.h"
#include <mcp_can.h>
#endif
#include <AsyncJson.h>

#define MEANWELL_MINIMAL_SET_VOLTAGE 42

#define MAX_CURRENT_MULTIPLIER 20

#define MEANWELL_SET_VOLTAGE 0x00
#define MEANWELL_SET_CURRENT 0x01
#define MEANWELL_SET_CURVE_CV 0x02
#define MEANWELL_SET_CURVE_CC 0x03
#define MEANWELL_SET_CURVE_FV 0x04
#define MEANWELL_SET_CURVE_TC 0x05

typedef struct {
    uint8_t operation; // 00h(OFF), 01h(ON), default 01h(ON)

    float outputVoltageSet; // 48V, read 0 ~ 80V, ±0.48V, write 42 ~ 80V, ±0.48V, default 0V
    float outputCurrentSet; // NPB-1200-48V, read, 0 ~ 22A, ±0.18A, write 3.6 ~ 18A, ±0.18A, default 18A
    float outputVoltage; // 48V, read 0 ~ 80V, ±0.48V, write 42 ~ 80V, Â±0.48V, default 0V
    float outputCurrent; // NPB-1200-48V, read, 0 ~ 22A, ±0.18A, write 3.6 ~ 18A, ±0.18A, default 18A
                         // NBP-750-48V, read, 0 ~ 14A, ±0.11A, write 2.26 ~ 11.3A, ±0.11A, default 11.3A
    float outputPower;
    union {
        uint16_t FaultStatus;
        struct {
            unsigned dummyFS1 : 1;
            unsigned OTP : 1; // Over temperature protection, 1 = Internal temperature abnormal
            unsigned OVP : 1; // Output over voltage protection, 1 = Output voltage protected, over xxx Volt
            unsigned OCP : 1; // Output over current protection, 1 = Output current protected
            unsigned SHORT : 1; // Output short circuit protection, 1 = Output shorted circuit protected
            unsigned AC_Fail : 1; // AC abnormal flag, 1 = AC abnormal protection, input under 80V
            unsigned OP_OFF : 1; // Output status, 0 = Output turned on, 1 = Output turned off
            unsigned HI_TEMP : 1; // Internal high temperature protection, 1 = Internal temperature abnormal
            unsigned dummyFS2:8;
        } FAULT_STATUS;
    };

    float inputVoltage; // 80 ~ 264V, ±10V
    float inputPower; // calculated from output power
    float efficiency;
    float internalTemperature; // -40 ~ 110°C, ±5°C

    char ManufacturerName[13];
    char ManufacturerModelName[13];
    uint8_t FirmwareRevision[6];
    char ManufacturerFactoryLocation[4];
    char ManufacturerDate[7];
    char ProductSerialNo[13];

    float curveCC; // NPB-750-48V, 2.26 ~ 11.3A, ±0.11A, default 11.3A
                   // NBP-1200-48V, 3.6 ~ 18A, ±0.18V, default 18A
    float curveCV; // 48V, VBST 42 ~ 80V, ±0.48V, default 57.6V
    float curveFV; // NBP-xxx-48V, VFLOAT 42 ~ VBST, ±0.48V, default 55.2V
    float curveTC; // NBP-750-48V, 0.23 ~ 3.39A, ±0.11A, default 1.13A
                   // NBP-1200-48V, 0.36 ~ 5.4A, ±0.18A, default 1.8A
    uint16_t curveCC_Timeout; // 60 ~ 64800 minutes, ±5 minute, default 600 minute
    uint16_t curveCV_Timeout; // 60 ~ 64800 minutes, ±5 minute, default 600 minute
    uint16_t curveFV_Timeout; // 60 ~ 64800 minutes, ±5 minute, default 600 minute

    union {
        uint16_t CurveConfig; // default 0x0004
        struct {
            unsigned CUVS : 2; // Charge curve setting;
                               // 00 = Customized charging curve(default)
                               // 01 = Preset charging curve#1
                               // 10 = Preset charging curve #2
                               // 11 = Preset charging curve #3
            unsigned TCS : 2; // Temperature compensation setting
                              // 00 = disable
                              // 01 = -3mV/°C/cell(default)
                              // 10 = -4mV/°C/cell
                              // 11 = -5mV/°C/cell
            unsigned dummyCC1 : 2;
            unsigned STGS : 1; // 2/3 stage charge setting
                               // 0 = 3 stage charge(dfault, CURVE_CV and CURVE_FV)
                               // 1 = 2 stage charge(only CURVE_CV)
            unsigned CUVE : 1; // Charge curve function enable
                               // 0 = Disabled, power supply mode
                               // 1 = Enabled, charger mode(defaut)
            unsigned CCTOE : 1; // Constant current stage timeout indication enable, 0 = Disabled(default), 1 = Enabled
            unsigned CVTOE : 1; // Constant voltage stage timeout indication enable, 0 = Disabled(default), 1 = Enabled
            unsigned FVTOE : 1; // Float stage timeout indication enable, 0 = Disabled(default), 1 = Enabled
            unsigned dummyCC2:5;
        } CURVE_CONFIG;
    };

    union {
        uint16_t ChargeStatus;
        struct {
            unsigned FULLM : 1; // Fully charged status, 1 = Fully charged
            unsigned CCM : 1; // Constant current mode status, 1 = The charger in constant current mode
            unsigned CVM : 1; // Constant voltage mode status, 1 = The charger in constant voltage mode
            unsigned FVM : 1; // Float mode status, 1 = The charger in float mode
            unsigned dummyCS1 : 2;
            unsigned WAKEUP_STOP : 1; // Wake up finished, 1ï¼Wake up unfinished
            unsigned dummyCS2 : 1;
            unsigned dummyCS3 : 2;
            unsigned NTCER : 1; // Temperature compensation status, 1=The circuitry of temperature compensation has short-circuited
            unsigned BTNC : 1; // Battery detection, 1=No battery detected
            unsigned dummyCS4 : 1;
            unsigned CCTOF : 1; // Timeout flag of constant current mode, 1=Constant current mode time out
            unsigned CVTOF : 1; // Timeout flag of constant voltage mode, 1=Constant voltage mode time out
            unsigned FVTOF : 1; // Timeout flag of float mode, 1=Float mode time out
        } CHG_STATUS;
    };

    uint16_t scalingFactor; // Scaling ratio

    union {
        uint16_t SystemStatus;
        struct {
            unsigned dummySS1 : 1;
            unsigned DC_OK : 1; // The DC output status, 0 = DC output at a normal range, 1 = DC output too low
            unsigned dummySS2 : 3;
            unsigned INITIAL_STATE : 1; // Initial stage indication, 0 = The unit NOT in an initial state, 1 = The unit in an initial state
            unsigned EEPER : 1; // EEPROM access Error, 0 = EEPROM accessing normally, 1 = EEPROM access error
                                // NOTE: EEPER:When EEPROM access error the supply stops working and the
                                // LED indicator turns off. The supply need to re-power on to recover after
                                // the error condition is removed.
            unsigned dummySS3 : 1;
        } SYSTEM_STATUS;
    };

    union {
        uint16_t SystemConfig; // System configuration, default 0x0002
        struct {
            unsigned CAN_CTRL : 1;
            unsigned OPERATION_INIT : 2; // Initial operational behavior
                                         // 00 = Power on with 00h: OFF
                                         // 01 = Power on with 01h: ON, default
                                         // 10 = Power on with the last setting
                                         // 11 = No used
            unsigned dummySC2 : 5;
            unsigned EEP_CONFIG : 2;
            unsigned EEP_OFF : 1;
            unsigned dummySC3 : 5;
        } SYSTEM_CONFIG;
    };
} RectifierParameters_t;

class MeanWellCanClass {
public:
    MeanWellCanClass();
    void init(Scheduler& scheduler);
    void updateSettings();
    void setAutomaticChargeMode(bool mode) { _automaticCharge = mode; };
    void setValue(float in, uint8_t parameterType);
    void setPower(bool power);

    uint32_t getLastUpdate() { return _lastUpdate; };
    bool getAutoPowerStatus() { return _automaticCharge ? (_rp.operation?true:false):false; };

    RectifierParameters_t _rp {};

    void generateJsonResponse(JsonVariant& root);

    bool getLastPowerCommandSuccess(void) { return _lastPowerCommandSuccess; };

    bool getVerboseLogging(void) { return _verboseLogging; };
    void setVerboseLogging(bool logging) { _verboseLogging = logging; };

    bool updateAvailable(uint32_t since) const;

    uint32_t getEEPROMwrites() { return EEPROMwrites;}

private:
    void loop();

    Task _loopTask;

    void updateEEPROMwrites2NVS();

    void switchChargerOff(const char* reason);
    bool enable(void);
    bool getCanCharger(void);
    bool parseCanPackets(void);
    void calcPower();
    void onReceive(uint8_t* frame, uint8_t len);
    const char* Word2BinaryString(uint16_t w);
    uint8_t* Float2Uint(float value);
    uint8_t* Bool2Byte(bool value);
    uint16_t readUnsignedInt16(uint8_t* data);
    int16_t readSignedInt16(uint8_t* data) { return readUnsignedInt16(data); }
    float scaleValue(int16_t value, float factor) { return value * factor; }
    bool sendCmd(uint8_t id, uint16_t cmd, uint8_t* data = reinterpret_cast<uint8_t*>(NULL), int len = 0);
    bool _sendCmd(uint8_t id, uint16_t cmd, uint8_t* data = reinterpret_cast<uint8_t*>(NULL), int len = 0);
    bool readCmd(uint8_t id, uint16_t cmd);
    float calcEfficency(float x);
    void setupParameter(void);

#ifdef CHARGER_USE_CAN0
    twai_general_config_t g_config;
#else
    SPIClass* spi;
    MCP_CAN* CAN;
    uint8_t _mcp2515_irq;
#endif

    enum class NPB_Model_t {
        NPB_450_24,
        NPB_450_48,
        NPB_750_24,
        NPB_750_48,
        NPB_1200_24,
        NPB_1200_48,
        NPB_1700_24,
        NPB_1700_48,
        NPB_unknown
    };
    NPB_Model_t _model = NPB_Model_t::NPB_450_48;

    uint32_t _previousMillis;
    uint32_t _lastUpdate;
    uint32_t _meanwellLastResponseTime = 0;

    bool _initialized = false;
    bool _automaticCharge = true;
    bool _lastPowerCommandSuccess;
    bool _setupParameter = true;

    bool _verboseLogging = false;

    const uint8_t ChargerID = 0x03;
    uint32_t EEPROMwrites;
};

extern MeanWellCanClass MeanWellCan;

#endif
