/* frameHandler.h
 *
 * Arduino library to read from REFUsol devices using Siemens RS485 USS protocol.
 * Derived from Siemens USS framehandler reference implementation.
 *
 * 2020.05.05 - 0.2 - initial release
 * 2022.08.20 - 0.4 - changes for OpenDTU
 *
 */

#ifdef USE_REFUsol_INVERTER

#pragma once

#include <Arduino.h>
#include <AsyncJson.h>

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#ifndef REFUsol_PIN_TX
#define REFUsol_PIN_TX 16 // HardwareSerial TX Pin
#endif

#ifndef REFUsol_PIN_RX
#define REFUsol_PIN_RX 17 // HardwareSerial RX Pin
#endif

#ifndef REFUsol_PIN_CTS
#define REFUsol_PIN_CTS -1 // HardwareSerial CTS Pin
#endif

#ifndef REFUsol_PIN_RTS
#define REFUsol_PIN_RTS 4 // HardwareSerial RTS Pin
#endif

#define REFUsol_MAX_NAME_LEN 9 // REFUsol Protocol: max name size is 9 including /0
#define REFUsol_MAX_VALUE_LEN 33 // REFUsol Protocol: max value size is 33 including /0

typedef struct {
    char serNo[REFUsol_MAX_VALUE_LEN]; // serial number
    char firmware[REFUsol_MAX_VALUE_LEN]; // firmware release number
    uint8_t currentStateOfOperation; // current state of operation e. g. OFF or Bulk
    uint8_t error; // error code
    uint8_t status; // status code
    uint32_t totalOperatingHours;
    float acVoltage; // ac voltage in V
    float acVoltageL1; // ac voltage L1 in V
    float acVoltageL2; // ac voltage L2 in V
    float acVoltageL3; // ac voltage L3 in V
    float acCurrent; // ac current in A
    float acCurrentL1; // ac current L1 in A
    float acCurrentL2; // ac current L2 in A
    float acCurrentL3; // ac current L3 in A
    float acPower; // ac power in W
    float freqL1;
    float freqL2;
    float freqL3;
    float dcPower; // panel power in W
    float dcVoltage; // panel voltage in V
    float dcCurrent; // panel current in A
    float YieldDay; // yield today kWh
    float maxPowerToday; // maximum power today W
    float YieldYesterday; // yield yesterday kWh
    float maxPowerYesterday; // maximum power yesterday W
    float YieldMonth; // yield month kWh
    float YieldYear; // yield year kWh
    float YieldTotal; // yield total kWh
    float pvPeak; // PV peak
    float pvLimit; // PV limit
    float deviceSpecificOffset;
    float plantSpecificOffset;
    float cosPhi;
    float effectivCosPhi;
    float variableOffset;
    float fixedOffset;
    int32_t optionCosPhi;
    float temperatureExtern;
    float temperatureRight;
    float temperatureTopLeft;
    float temperatureBottomRight;
    float temperatureLeft;
} REFUsolStruct;

#pragma pack(push, 1)
typedef union {
    uint32_t addr;
    struct {
        uint8_t a1;
        uint8_t a2;
        uint8_t a3;
        uint8_t a4;
    };
} IPADDR_t;

typedef union {
    uint16_t W;
    struct {
        uint8_t H;
        uint8_t L;
    };
} WORD_t;

typedef union {
    struct {
        uint8_t STX;
        uint8_t LGE;
        uint8_t ADR;
        union {
            struct {
                union {
                    unsigned long long PKW;
                    struct {
                        WORD_t PKE;
                        WORD_t IND;
                        WORD_t PWE1;
                        WORD_t PWE2;
                    };
                };
                WORD_t PZD;
            };
            uint8_t net[252];
        };
        uint8_t BCC;
    };
    uint8_t buffer[256];
} TELEGRAM_t;
#pragma pack(pop)

#define RequestPWE ((uint16_t)0b0001)
#define ChangePWEword ((uint16_t)0b0010)
#define ChangePWEdword ((uint16_t)0b0011)
#define RequestPWEarray ((uint16_t)0b0110)
#define ChangePWEarray ((uint16_t)0b0111)
#define ChangePWEdarray ((uint16_t)0b1000)
#define RequestNumArrayElements ((uint16_t)0b1001)
#define RequestText ((uint16_t)0b000001111)
#define ChangeText ((uint16_t)0b100001111)

#define NaN -9999999999.0

class REFUsolRS485ReceiverClass {

public:
    REFUsolRS485ReceiverClass();
    void init(Scheduler& scheduler); // initialize HardewareSerial
    void deinit(void);
    unsigned long getLastUpdate(); // timestamp of last successful frame read
    bool isDataValid(void); // return true if data valid and not outdated

    REFUsolStruct Frame {}; // public struct for received name and value pairs

    void generateJsonResponse(JsonVariant& root);

    bool getVerboseLogging(void) { return _verbose_logging; };
    void setVerboseLogging(bool logging) { _verbose_logging = logging; };

private:
    void loop(void); // main loop to read ve.direct data

    Task _loopTask;

    void parse(void);
    void getDCpower(uint8_t adr);
    void getDCvoltage(uint8_t adr);
    void getDCcurrent(uint8_t adr);
    void getACpower(uint8_t adr);
    void getACvoltage(uint8_t adr, uint16_t ind = 0);
    void getACcurrent(uint8_t adr, uint16_t ind = 0);
    void getACfrequency(uint8_t adr, uint16_t ind = 0);
    void getTotalOperatingHours(uint8_t adr);
    void getYieldDay(uint8_t adr);
    void getYieldMonth(uint8_t adr);
    void getYieldYear(uint8_t adr);
    void getYieldTotal(uint8_t adr);
    void getPVpeak(uint8_t adr);
    void getPVlimit(uint8_t adr);
    void getDeviceSpecificOffset(uint8_t adr);
    void getPlantSpecificOffset(uint8_t adr);
    void getFixedOffset(uint8_t adr);
    void getVariableOffset(uint8_t adr);
    void getOptionCosPhi(uint8_t adr);
    void getCosPhi(uint8_t adr);
    void getTemperatursensor(uint8_t adr, uint16_t ind = 0);
    void getActualStatus(uint8_t adr);
    void getFehlercode(uint8_t adr);
    void getPassword(uint8_t adr);
    //    IPADDR_t *getIPaddress(uint8_t adr);
    //    IPADDR_t *getIPmask(uint8_t adr);
    //    IPADDR_t *getGateway(uint8_t adr);

    void setPKW_ANZ(uint8_t anz) { PKW_ANZ = anz; }
    void setPZD_ANZ(uint8_t anz) { PZD_ANZ = anz; }
    void compute(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD);
    void computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND);
    void computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE);
    void computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, void* PZD);
    void computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD);
    void sendTelegram(void);
    uint8_t receiveTelegram(void);
    float PWE2Float(float scale = 1.0);
    uint32_t PWE2Uint32(void);

    void setLastUpdate(); // set timestampt after successful frame read

    unsigned long _lastPoll;

    TELEGRAM_t sTelegram;
    TELEGRAM_t rTelegram;

    uint8_t PKW_ANZ = 4;
    uint8_t PZD_ANZ = 0;

    uint16_t RS485BaudRate = 57600;

    void DebugNoResponse(uint16_t p);
    bool DebugDecodedTelegram = true;
    void debugRawTelegram(TELEGRAM_t* t, uint8_t len);
    bool DebugRawTelegram = true;
    int MaximumResponseTime = 20;

    uint8_t adr = 1;

    bool _isInstalled = false;
    bool _initialized = false;

    bool _verbose_logging = false;
};

extern REFUsolRS485ReceiverClass REFUsol;

#endif
