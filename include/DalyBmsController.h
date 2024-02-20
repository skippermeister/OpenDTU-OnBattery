#ifdef USE_DALYBMS_CONTROLLER

#pragma once

#include "Battery.h"
#include "Configuration.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <TimeoutHelper.h>
#include <driver/uart.h>
#include <espMqttClient.h>
#include <memory>
#include <vector>
#include <frozen/string.h>

#define HwSerial Serial2

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

class DalyBmsController : public BatteryProvider {
    public:
        bool init() final;
        void deinit() final;
        void loop() final;
        std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

        void set_address(uint8_t address) { _addr = address; }

    private:
        static const uint8_t XFER_BUFFER_LENGTH = 13;
        static const uint8_t MIN_NUMBER_CELLS = 1;
        static const uint8_t MAX_NUMBER_CELLS = 48;
        static const uint8_t MIN_NUMBER_TEMP_SENSORS = 1;
        static const uint8_t MAX_NUMBER_TEMP_SENSORS = 16;

        static const uint8_t START_BYTE = 0xA5; // Start byte
        static const uint8_t HOST_ADRESS = 0x40; // Host address
        uint8_t _addr = HOST_ADRESS;

        static const uint8_t DALY_FRAME_SIZE = 13;
        static const uint8_t DALY_TEMPERATURE_OFFSET = 40;
        static const uint16_t DALY_CURRENT_OFFSET = 30000;

        // Parameter
        enum Command : uint8_t {
            REQUEST_RATED_CAPACITY_CELL_VOLTAGE        = 0x50,
            REQUEST_ACQUISITION_BOARD_INFO             = 0x51,
            REQUEST_CUMULATIVE_CAPACITY                = 0x52,
            REQUEST_BATTERY_TYPE_INFO                  = 0x53,
            REQUEST_FIRMWARE_INDEX                     = 0x54,
            REQUEST_IP                                 = 0x56,
            REQUEST_BATTERY_CODE                       = 0x57,
            REQUEST_MIN_MAX_CELL_VOLTAGE               = 0x59,
            REQUEST_MIN_MAX_PACK_VOLTAGE               = 0x5A,
            REQUEST_MAX_PACK_DISCHARGE_CHARGE_CURRENT  = 0x5B,
            REQUEST_MIN_MAX_SOC_LIMIT                  = 0x5D,
            REQUEST_VOLTAGE_TEMPERATURE_DIFFERENCE     = 0x5E,
            REQUEST_BALANCE_START_DIFF_VOLTAGE         = 0x5F,
            REQUEST_SHORT_CURRENT_RESISTANCE           = 0x60,
            REQUEST_RTC                                = 0x61,
            REQUEST_BMS_SW_VERSION                     = 0x62,
            REQUEST_BMS_HW_VERSION                     = 0x63,

            // actual values
            REQUEST_BATTERY_LEVEL       = 0x90,
            REQUEST_MIN_MAX_VOLTAGE     = 0x91,
            REQUEST_MIN_MAX_TEMPERATURE = 0x92,
            REQUEST_MOS                 = 0x93,
            REQUEST_STATUS              = 0x94,
            REQUEST_CELL_VOLTAGE        = 0x95,
            REQUEST_TEMPERATURE         = 0x96,
            REQUEST_CELL_BALANCE_STATES = 0x97,
            REQUEST_FAILURE_CODES       = 0x98,

            // settings
            WRITE_RTC_AND_SOC = 0x21, // bytes: YY MM DD hh mm ss soc_hi soc_low (0.1%)
            WRITE_DISCHRG_FET = 0xD9,
            WRITE_CHRG_FET    = 0xDA,
            WRITE_BMS_RESET   = 0x00
        };

        enum class Status : unsigned {
            Initializing,
            Timeout,
            WaitingForPollInterval,
            HwSerialNotAvailableForWrite,
            BusyReading,
            RequestSent,
            FrameCompleted
        };
        frozen::string const& getStatusText(Status status);
        void announceStatus(Status status);
        void sendRequest(uint8_t pollInterval);
        bool requestData(uint8_t cmdID);
        void rxData(uint8_t inbyte);
        void decodeData(std::vector<uint8_t> rxBuffer);

        void reset();
        void clearGet(void);

        Status _lastStatus = Status::Initializing;
        TimeoutHelper _lastStatusPrinted;
        bool _receiving = false;
        bool _triggerNext = false;
        uint32_t _lastResponse = 0;
        uint32_t _lastParameterReceived = 0;
        uint32_t _lastRequest = 0;
        bool _readParameter = true;
        uint8_t _nextRequest = 0;

        uint8_t _txBuffer[XFER_BUFFER_LENGTH];
        std::vector<uint8_t> _rxBuffer;

        #pragma pack(push, 1)
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
                uint8_t hh;
            };
        } int32Struct_t;
        #pragma pack(pop)
        uint32_t ToUint32(uint8_t* c)
        {
            int32Struct_t rc;
            rc.u = *c++;
            rc.hl = *c++;
            rc.lh = *c++;
            rc.ll = *c;
            return rc.u;
        }

        uint16_t ToUint16(uint8_t* c)
        {
            int16Struct_t rc;
            rc.h = *c++;
            rc.l = *c;
            return rc.u;
        }
        int16_t ToInt16(uint8_t* c)
        {
            int16Struct_t rc;
            rc.h = *c++;
            rc.l = *c;
            return rc.i;
        }
        float to_Volt(uint8_t* c) { return static_cast<float>(ToUint16(c)) / 1000.0; }

        bool _wasActive;

        std::shared_ptr<DalyBmsBatteryStats> _stats = std::make_shared<DalyBmsBatteryStats>();
};

#endif
