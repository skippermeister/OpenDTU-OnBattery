// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef USE_GOBEL_RS485_RECEIVER

#include "Battery.h"
#include <TimeoutHelper.h>
#include <espMqttClient.h>
#include <memory>

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#pragma pack(push, 1)
typedef struct {
    uint8_t ver; // HexToByte(construct.Array(2, construct.Byte)),
    uint8_t adr; // HexToByte(construct.Array(2, construct.Byte)),
    uint8_t cid1; // HexToByte(construct.Array(2, construct.Byte)),
    uint8_t cid2; // HexToByte(construct.Array(2, construct.Byte)),
    uint16_t infolength; // HexToByte(construct.Array(4, construct.Byte)),
    uint8_t info[256]; // HexToByte(construct.GreedyRange(construct.Byte))
} format_t;

typedef union {
    int16_t i;
    uint16_t u;
    struct {
        uint8_t l;
        uint8_t h;
    };
} int16Struct_t;

typedef union {
    int32_t i;
    uint32_t u;
    struct {
        uint8_t ll;
        uint8_t lh;
        uint8_t hl;
        uint8_t dummy;
    };
} int24Struct_t;
#pragma pack(pop)

class GobelRS485Receiver : public BatteryProvider {
    enum Function : uint8_t
    {
        REQUEST = 0,
        REQUEST_AND_GET = 1,
        GET = 2
    };

    enum Command : uint8_t
    {
        None = 0,
        GetAnalogValue = 0x42,
        GetAlarmInfo = 0x44,
        GetSystemParameter = 0x47,
        GetProtocolVersion = 0x4F,
        GetManufacturerInfo = 0x51,
        GetSystemBasicInformation = 0x60,    // Protocoll Document V3.5
        GetSystemAnalogData = 0x61,          // Protocoll Document V3.5
        GetSystemAlarmInfo = 0x62,           // Protocoll Document V3.5
        GetSystemChargeDischargeManagementInfo = 0x63, // Protocoll Document V3.5
        SystemShutdown = 0x64,  // Protocoll Document V3.5
        GetPackCount = 0x90,
        GetChargeDischargeManagementInfo = 0x92,
        GetSerialNumber = 0x93,
        SetChargeDischargeManagementInfo = 0x94,
        TurnOffModule = 0x95,
        GetFirmwareInfo = 0x96,
        ControlCommand = 0x99, // INFO=0x0C Buzzer off, INFO=0x0D Buzzer on
        ChargeMOSFETcontrol = 0x9A,
        DischargeMOSFETcontrol = 0x9B,
        GetPackCapacity = 0xA6,
        BMSTime = 0xB1,
        GetVersionInfo = 0xC1,
        GetBarCode = 0xC2,
        GetCellOV = 0xD1,
        StartCurrent = 0xED
    };

    enum ResponseCode
    {
        Normal = 0x00,
        VER_error = 0x01,
        CHKSM_error = 0x02,
        LCHKSUM_error = 0x03,
        CID2_invalid = 0x04,
        Command_format_error = 0x05,
        Invalid_data = 0x06,
        Operation_or_write_error = 0x09,
        ADR_error = 0x90,
        Communication_error = 0x91
    };

public:
    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    bool initialized() const final { return _initialized; };

private:
    static char constexpr _serialPortOwner[] = "Gobel";

    std::unique_ptr<HardwareSerial> _upSerial;

    void readParameter();

    void get_protocol_version(const GobelRS485Receiver::Function function, uint8_t module);
    void get_manufacturer_info(const GobelRS485Receiver::Function function, uint8_t module);
    void get_barcode(const GobelRS485Receiver::Function function, uint8_t module);
    bool get_pack_count(const GobelRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand = 1);
    void get_version_info(const GobelRS485Receiver::Function function, uint8_t module);
    void get_firmware_info(const GobelRS485Receiver::Function function, uint8_t module);
    void get_analog_value(const GobelRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand = 1);
    void get_system_parameters(const GobelRS485Receiver::Function function, uint8_t module);
    void get_alarm_info(const GobelRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand = 1);
    void get_charge_discharge_management_info(const GobelRS485Receiver::Function function, uint8_t module);
    void get_module_serial_number(const GobelRS485Receiver::Function function, uint8_t module);
    void setting_charge_discharge_management_info(const GobelRS485Receiver::Function function, uint8_t module = 2, const char* info = NULL);
    void turn_off_module(const GobelRS485Receiver::Function function, uint8_t module);
    void get_pack_capacity(const GobelRS485Receiver::Function function);

    uint16_t get_frame_checksum(char* frame);
    uint16_t get_info_length(const char* info);
    //void _encode_cmd(char *frame, uint8_t address, uint8_t cid2, const char* info);
    void send_cmd(uint8_t address, uint8_t cmnd, uint8_t InfoCommand = 1);
    size_t readline(void);
    uint32_t hex2binary(char* s, int len);
    char* _decode_hw_frame(size_t length);
    format_t* _decode_frame(char* frame);
    format_t* read_frame(void);

    //    uint16_t readUnsignedInt16(uint8_t *data) { return ((*(data+1)) << 8) + (*data); };
    //    int16_t readSignedInt16(uint8_t *data) { return readUnsignedInt16(data); }
    float scaleValue(int16_t value, float factor) { return value * factor; }
    bool getBit(uint8_t value, uint8_t bit) { return (value & (1 << bit)) >> bit; }

    uint32_t ToUint24(uint8_t*& c)
    {
        int24Struct_t rc;
        rc.u = 0;
        rc.hl = *c++;
        rc.lh = *c++;
        rc.ll = *c++;
        return rc.u;
    }
    uint16_t ToUint16(uint8_t*& c)
    {
        int16Struct_t rc;
        rc.h = *c++;
        rc.l = *c++;
        return rc.u;
    }
    int16_t ToInt16(uint8_t*& c)
    {
        int16Struct_t rc;
        rc.h = *c++;
        rc.l = *c++;
        return rc.i;
    }
    float to_Celsius(uint8_t*& c) { return static_cast<float>(ToInt16(c) - 2731) / 10.0; }
    float to_Volt(uint8_t*& c) { return static_cast<float>(ToUint16(c)) / 1000.0; }
    float to_CellVolt(uint8_t*& c) { return static_cast<float>(ToInt16(c)) / 1000.0; }
    float to_Amp(uint8_t*& c) { return static_cast<float>(ToInt16(c)) / 10.0; }
    float to_AmpHour(uint8_t*& c) { return static_cast<float>(ToUint16(c)) / 10.0; }
    float DivideUint16By1000(uint8_t*& c) { return static_cast<float>(ToUint16(c)) / 1000.0; }
    float DivideUint24By1000(uint8_t*& c) { return static_cast<float>(ToUint24(c)) / 1000.0; }

    char *_received_frame;

    uint8_t _lastCmnd = 0xFF;

    uint8_t _masterBatteryID = 0;
    uint8_t _lastSlaveBatteryID = 0;

    esp_err_t twaiLastResult;

    TimeoutHelper _lastBatteryCheck;

    std::shared_ptr<GobelRS485BatteryStats> _stats = std::make_shared<GobelRS485BatteryStats>();

    bool _initialized = false;
};

#endif
