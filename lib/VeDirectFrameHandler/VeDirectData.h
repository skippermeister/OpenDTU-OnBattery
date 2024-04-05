#pragma once

#include <frozen/string.h>
#include <frozen/map.h>

#define VE_MAX_VALUE_LEN 33 // VE.Direct Protocol: max value size is 33 including /0
#define VE_MAX_HEX_LEN 100 // Maximum size of hex frame - max payload 34 byte (=68 char) + safe buffer

typedef struct {
    uint16_t PID = 0;               // product id
    char SER[VE_MAX_VALUE_LEN];     // serial number
    char FW[VE_MAX_VALUE_LEN];      // firmware release number
    float V = 0;                    // battery voltage in V
    float I = 0;                    // battery current in A
    float E = 0;                    // efficiency in percent (calculated, moving average)

    frozen::string const& getPidAsString() const; // product ID as string
} veStruct;

struct veMpptStruct : veStruct {
    uint8_t MPPT;                   // state of MPP tracker
    int32_t PPV;                    // panel power in W
    int32_t P;                      // battery output power in W (calculated)
    float VPV;                      // panel voltage in V
    float IPV;                      // panel current in A (calculated)
    bool LOAD;                      // virtual load output state (on if battery voltage reaches upper limit, off if battery reaches lower limit)
    float IL;                       // output current (Lastausgang)
    uint8_t CS;                     // current state of operation e.g. OFF or Bulk
    uint8_t ERR;                    // error code
    uint32_t OR;                    // off reason
    uint32_t HSDS;                  // day sequence number 1...365
    float H19;                      // yield total kWh
    float H20;                      // yield today kWh
    int32_t H21;                    // maximum power today W
    float H22;                      // yield yesterday kWh
    int32_t H23;                    // maximum power yesterday W

    // these are values communicated through the HEX protocol. the pair's first
    // value is the timestamp the respective info was last received. if it is
    // zero, the value is deemed invalid. the timestamp is reset if no current
    // value could be retrieved.
    std::pair<uint32_t, uint32_t> Capabilities;
    std::pair<uint32_t, int32_t> MpptTemperatureMilliCelsius;
    std::pair<uint32_t, int32_t> SmartBatterySenseTemperatureMilliCelsius;
    std::pair<uint32_t, uint8_t> LoadOutputState;
    std::pair<uint32_t, uint8_t> LoadOutputControl;
    std::pair<uint32_t, uint32_t> LoadOutputVoltage;
    std::pair<uint32_t, uint16_t> LoadCurrent;
    std::pair<uint32_t, uint32_t> ChargerVoltage;
    std::pair<uint32_t, uint32_t> ChargerCurrent;
    std::pair<uint32_t, uint32_t> ChargerMaximumCurrent;
    std::pair<uint32_t, uint16_t> VoltageSettingsRange;
    std::pair<uint32_t, uint32_t> NetworkTotalDcInputPowerMilliWatts;
    std::pair<uint32_t, uint8_t> DeviceMode;
    std::pair<uint32_t, uint8_t> RemoteControlUsed;
    std::pair<uint32_t, uint8_t> DeviceState;
    std::pair<uint32_t, uint32_t> BatteryMaximumCurrent;
    std::pair<uint32_t, uint32_t> BatteryAbsorptionVoltage;
    std::pair<uint32_t, uint32_t> BatteryFloatVoltage;
    std::pair<uint32_t, uint8_t> BatteryType;
    std::pair<uint32_t, uint8_t> NetworkInfo;
    std::pair<uint32_t, uint8_t> NetworkMode;
    std::pair<uint32_t, uint8_t> NetworkStatus;

    frozen::string const& getMpptAsString() const; // state of mppt as string
    frozen::string const& getCsAsString() const;   // current state as string
    frozen::string const& getErrAsString() const;  // error state as string
    frozen::string const& getOrAsString() const;   // off reason as string
    frozen::string const& getCapabilitiesAsString(uint8_t bit) const;   //
    frozen::string const& getBatteryTypeAsString() const;   //
};

struct veShuntStruct : veStruct {
    int32_t T;                      // Battery temperature
    bool tempPresent;               // Battery temperature sensor is attached to the shunt
    int32_t P;                      // Instantaneous power
    int32_t CE;                     // Consumed Amp Hours
    int32_t SOC;                    // State-of-charge
    uint32_t TTG;                   // Time-to-go
    bool ALARM;                     // Alarm condition active
    uint32_t AR;                    // Alarm Reason
    int32_t H1;                     // Depth of the deepest discharge
    int32_t H2;                     // Depth of the last discharge
    int32_t H3;                     // Depth of the average discharge
    int32_t H4;                     // Number of charge cycles
    int32_t H5;                     // Number of full discharges
    int32_t H6;                     // Cumulative Amp Hours drawn
    int32_t H7;                     // Minimum main (battery) voltage
    int32_t H8;                     // Maximum main (battery) voltage
    int32_t H9;                     // Number of seconds since last full charge
    int32_t H10;                    // Number of automatic synchronizations
    int32_t H11;                    // Number of low main voltage alarms
    int32_t H12;                    // Number of high main voltage alarms
    int32_t H13;                    // Number of low auxiliary voltage alarms
    int32_t H14;                    // Number of high auxiliary voltage alarms
    int32_t H15;                    // Minimum auxiliary (battery) voltage
    int32_t H16;                    // Maximum auxiliary (battery) voltage
    int32_t H17;                    // Amount of discharged energy
    int32_t H18;                    // Amount of charged energy
};


enum class VeDirectHexCommand : uint8_t {
    ENTER_BOOT = 0x0,
    PING = 0x1,
    RSV1 = 0x2,
    APP_VERSION = 0x3,
    PRODUCT_ID = 0x4,
    RSV2 = 0x5,
    RESTART = 0x6,
    GET = 0x7,
    SET = 0x8,
    RSV3 = 0x9,
    ASYNC = 0xA,
    RSV4 = 0xB,
    RSV5 = 0xC,
    RSV6 = 0xD,
    RSV7 = 0xE,
    RSV8 = 0xF
};

enum class VeDirectHexResponse : uint8_t {
    DONE = 0x1,
    UNKNOWN = 0x3,
    ERROR = 0x4,
    PING = 0x5,
    GET = 0x7,
    SET = 0x8,
    ASYNC = 0xA
};

enum class VeDirectHexRegister : uint16_t {
    Capabilities = 0x0140,
    DeviceMode = 0x0200,
    DeviceState = 0x0201,
    RemoteControlUsed = 0x0202,
    PanelVoltage = 0xEDBB,
    ChargerVoltage = 0xEDD5,
    ChargerCurrent = 0xEDD7,
    ChargerMaximumCurrent = 0xEDDF,
    VoltageSettingsRange = 0xEDCE,  // union { uint16_t value; struct {uint8_t minSystemVoltage; uint8_t maxSystemVoltage;} }
    NetworkTotalDcInputPower = 0x2027,
    LoadOutputState = 0xEDA8,
    LoadOutputVoltage = 0xEDA9,
    LoadOutputControl = 0xEDAB,
    LoadCurrent = 0xEDAD,
    BatteryMaximumCurrent = 0xEDF0,
    BatteryAbsorptionVoltage = 0xEDF7,
    BatteryFloatVoltage = 0xEDF6,
    BatteryType = 0xEDF1,
    ChargeControllerTemperature = 0xEDDB,
    SmartBatterySenseTemperature = 0xEDEC,
    NetworkInfo = 0x200D,
    NetworkMode = 0x200E,
    NetworkStatus = 0x200F
};

struct VeDirectHexData {
    VeDirectHexResponse rsp;        // hex response code
    VeDirectHexRegister addr;       // register address
    uint8_t flags;                  // flags
    uint32_t value;                 // integer value of register
    char text[VE_MAX_HEX_LEN];      // text/string response

    frozen::string const& getResponseAsString() const;
    frozen::string const& getRegisterAsString() const;
};
