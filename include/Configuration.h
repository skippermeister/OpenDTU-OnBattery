// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "PinMapping.h"
#include <cstdint>

#define CONFIG_FILENAME "/config.json"
#define CONFIG_VERSION 0x00011b00 // 0.1.27 // make sure to clean all after change

#define WIFI_MAX_SSID_STRLEN 32
#define WIFI_MAX_PASSWORD_STRLEN 64
#define WIFI_MAX_HOSTNAME_STRLEN 31

#define NTP_MAX_SERVER_STRLEN 31
#define NTP_MAX_TIMEZONE_STRLEN 50
#define NTP_MAX_TIMEZONEDESCR_STRLEN 50

#define MQTT_MAX_HOSTNAME_STRLEN 128
#define MQTT_MAX_USERNAME_STRLEN 64
#define MQTT_MAX_PASSWORD_STRLEN 64
#define MQTT_MAX_TOPIC_STRLEN 256
#define MQTT_MAX_LWTVALUE_STRLEN 20
#define MQTT_MAX_CERT_STRLEN 2560

#define INV_MAX_NAME_STRLEN 31
#define INV_MAX_COUNT 3 // FIXME original 5
#define INV_MAX_CHAN_COUNT 6

#define CHAN_MAX_NAME_STRLEN 31

#define DEV_MAX_MAPPING_NAME_STRLEN 63

#define VICTRON_MAX_COUNT 2

#define POWERMETER_MAX_PHASES 3
#define POWERMETER_MAX_HTTP_URL_STRLEN 256 // 1024
#define POWERMETER_MAX_USERNAME_STRLEN 64
#define POWERMETER_MAX_PASSWORD_STRLEN 64
#define POWERMETER_MAX_HTTP_HEADER_KEY_STRLEN 64
#define POWERMETER_MAX_HTTP_HEADER_VALUE_STRLEN 256
#define POWERMETER_MAX_HTTP_JSON_PATH_STRLEN 256
#define POWERMETER_HTTP_TIMEOUT 2000 // FIXME original 1000

#ifdef USE_LED_SINGLE
    #define LED_COUNT   PINMAPPING_LED_COUNT
#endif
#ifdef USE_LED_STRIP
    #define LED_COUNT   2
#endif

#define JSON_BUFFER_SIZE 16 * 1024

struct CHANNEL_CONFIG_T {
    uint16_t MaxChannelPower;
    char Name[CHAN_MAX_NAME_STRLEN];
    float YieldTotalOffset;
};

struct INVERTER_CONFIG_T {
    uint64_t Serial;
    char Name[INV_MAX_NAME_STRLEN + 1];
    uint8_t Order;
    bool Poll_Enable_Day;
    bool Poll_Enable_Night;
    bool Command_Enable_Day;
    bool Command_Enable_Night;
    uint8_t ReachableThreshold;
    bool ZeroRuntimeDataIfUnrechable;
    bool ZeroYieldDayOnMidnight;
    bool YieldDayCorrection;
    CHANNEL_CONFIG_T channel[INV_MAX_CHAN_COUNT];
};

typedef struct {
    uint64_t Serial;
    uint32_t PollInterval;
    struct {
        int8_t PaLevel;
    } Nrf;
    struct {
        int8_t PaLevel;
        uint32_t Frequency;
        uint8_t CountryMode;
    } Cmt;
} Dtu_CONFIG_T;

enum Auth { none,
    basic,
    digest };
enum PowerMeterUnits { kW,
    W,
    mW };
struct POWERMETER_HTTP_PHASE_CONFIG_T {
    bool Enabled;
    char Url[POWERMETER_MAX_HTTP_URL_STRLEN + 1];
    Auth AuthType;
    char Username[POWERMETER_MAX_USERNAME_STRLEN + 1];
    char Password[POWERMETER_MAX_USERNAME_STRLEN + 1];
    char HeaderKey[POWERMETER_MAX_HTTP_HEADER_KEY_STRLEN + 1];
    char HeaderValue[POWERMETER_MAX_HTTP_HEADER_VALUE_STRLEN + 1];
    uint16_t Timeout;
    char JsonPath[POWERMETER_MAX_HTTP_JSON_PATH_STRLEN + 1];
    PowerMeterUnits Unit;
};

struct WiFi_CONFIG_T {
    char Ssid[WIFI_MAX_SSID_STRLEN + 1];
    char Password[WIFI_MAX_PASSWORD_STRLEN + 1];
    uint8_t Ip[4];
    uint8_t Netmask[4];
    uint8_t Gateway[4];
    uint8_t Dns1[4];
    uint8_t Dns2[4];
    bool Dhcp;
    char Hostname[WIFI_MAX_HOSTNAME_STRLEN + 1];
    uint32_t ApTimeout;
};

struct Mdns_CONFIG_T {
    bool Enabled;
};

struct Modbus_CONFIG_T {
    bool Fronius_SM_Simulation_Enabled;
};

struct Ntp_CONFIG_T {
    char Server[NTP_MAX_SERVER_STRLEN + 1];
    char Timezone[NTP_MAX_TIMEZONE_STRLEN + 1];
    char TimezoneDescr[NTP_MAX_TIMEZONEDESCR_STRLEN + 1];
    double Longitude;
    double Latitude;
    uint8_t SunsetType;
};

struct PowerMeter_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
    uint32_t PollInterval;
    uint32_t Source;
    struct {
        char TopicPowerMeter1[MQTT_MAX_TOPIC_STRLEN + 1];
        char TopicPowerMeter2[MQTT_MAX_TOPIC_STRLEN + 1];
        char TopicPowerMeter3[MQTT_MAX_TOPIC_STRLEN + 1];
    } Mqtt;
    struct {
        uint32_t Baudrate;
        uint32_t Address;
    } Sdm;
    struct {
        //    uint32_t Interval;
        bool IndividualRequests;
        POWERMETER_HTTP_PHASE_CONFIG_T Phase[POWERMETER_MAX_PHASES];
    } Http;
};

struct PowerLimiter_CONFIG_T {
    bool Enabled;
    bool SolarPassThroughEnabled;
    uint8_t SolarPassThroughLosses;
    bool BatteryAlwaysUseAtNight;
    bool UpdatesOnly;
    uint32_t PollInterval;
    bool IsInverterBehindPowerMeter;
    bool IsInverterSolarPowered;
    uint64_t InverterId;
    uint8_t InverterChannelId;
    int32_t TargetPowerConsumption;
    int32_t TargetPowerConsumptionHysteresis;
    int32_t LowerPowerLimit;
    int32_t UpperPowerLimit;
    bool IgnoreSoc;
    uint32_t BatterySocStartThreshold;
    uint32_t BatterySocStopThreshold;
    float VoltageStartThreshold;
    float VoltageStopThreshold;
    float VoltageLoadCorrectionFactor;
    int8_t RestartHour;
    uint32_t FullSolarPassThroughSoc;
    float FullSolarPassThroughStartVoltage;
    float FullSolarPassThroughStopVoltage;
};

struct Battery_CONFIG_T {
    bool Enabled;
    uint8_t numberOfBatteries;
    uint32_t PollInterval;
    uint8_t Provider;
#ifdef USE_JKBMS_CONTROLLER
    struct {
        uint8_t Interface;
        uint8_t PollingInterval;
    } JkBms;
#endif
#ifdef USE_MQTT_BATTERY
    struct {
        char SocTopic[MQTT_MAX_TOPIC_STRLEN + 1];
        char VoltageTopic[MQTT_MAX_TOPIC_STRLEN + 1];
    } Mqtt;
#endif
    bool UpdatesOnly;
    int8_t MinChargeTemperature;
    int8_t MaxChargeTemperature;
    int8_t MinDischargeTemperature;
    int8_t MaxDischargeTemperature;
};

struct Mqtt_CONFIG_T {
    bool Enabled;
    char Hostname[MQTT_MAX_HOSTNAME_STRLEN + 1];
    uint32_t Port;
    char Username[MQTT_MAX_USERNAME_STRLEN + 1];
    char Password[MQTT_MAX_PASSWORD_STRLEN + 1];
    char Topic[MQTT_MAX_TOPIC_STRLEN + 1];
    bool Retain;
    uint32_t PublishInterval;
    bool CleanSession;
    struct {
        char Topic[MQTT_MAX_TOPIC_STRLEN + 1];
        char Value_Online[MQTT_MAX_LWTVALUE_STRLEN + 1];
        char Value_Offline[MQTT_MAX_LWTVALUE_STRLEN + 1];
        uint8_t Qos;
    } Lwt;
#ifdef USE_HASS
    struct {
        bool Enabled;
        bool Retain;
        char Topic[MQTT_MAX_TOPIC_STRLEN + 1];
        bool IndividualPanels;
        bool Expire;
    } Hass;
#endif
    struct {
        bool Enabled;
        char RootCaCert[MQTT_MAX_CERT_STRLEN + 1];
        bool CertLogin;
        char ClientCert[MQTT_MAX_CERT_STRLEN + 1];
        char ClientKey[MQTT_MAX_CERT_STRLEN + 1];
    } Tls;
};

struct Huawei_CONFIG_T {
    bool Enabled;
    bool Auto_Power_Enabled;
    float Auto_Power_Voltage_Limit;
    float Auto_Power_Enable_Voltage_Limit;
    float Auto_Power_Lower_Power_Limit;
    float Auto_Power_Upper_Power_Limit;
};

struct MeanWell_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
    uint32_t PollInterval;
    float MinVoltage;
    float MaxVoltage;
    float MinCurrent;
    float MaxCurrent;
    float Hysteresis;
    bool mustInverterProduce;
    uint32_t EEPROMwrites;
};

struct Vedirect_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
};

struct REFUsol_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
    uint32_t PollInterval;
};

struct ZeroExport_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
    uint16_t InverterId; // mask
    uint64_t serials[INV_MAX_COUNT];
    uint16_t MaxGrid;
    uint16_t PowerHysteresis;
    uint16_t MinimumLimit;
    uint16_t Tn;
};

#ifdef USE_DISPLAY_GRAPHIC
struct Display_CONFIG_T {
    bool PowerSafe;
    bool ScreenSaver;
    uint8_t Rotation;
    uint8_t Contrast;
    uint8_t Language;
    struct {
        uint32_t Duration;
        uint8_t Mode;
    } Diagram;
};
#endif

struct Cfg_T {
    uint32_t Version;
    uint32_t SaveCount;
};

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
struct Led_Config_T {
        uint8_t Brightness;
};
#endif

struct CONFIG_T {
    Cfg_T Cfg;

    WiFi_CONFIG_T WiFi;
    Mdns_CONFIG_T Mdns;

#ifdef USE_ModbusDTU
    Modbus_CONFIG_T Modbus;
#endif

    Ntp_CONFIG_T Ntp;
    INVERTER_CONFIG_T Inverter[INV_MAX_COUNT];
    Dtu_CONFIG_T Dtu;
    Mqtt_CONFIG_T Mqtt;
    Vedirect_CONFIG_T Vedirect;

#ifdef USE_REFUsol_INVERTER
    REFUsol_CONFIG_T REFUsol;
#endif

    PowerMeter_CONFIG_T PowerMeter;
    PowerLimiter_CONFIG_T PowerLimiter;
    Battery_CONFIG_T Battery;
    struct {
        uint32_t Controller_Frequency;
    } MCP2515;

#ifdef CHARGER_HUAWEI
    Huawei_CONFIG_T Huawei;
#else
    MeanWell_CONFIG_T MeanWell;
#endif

    struct {
        char Password[WIFI_MAX_PASSWORD_STRLEN + 1];
        bool AllowReadonly;
    } Security;

    char Dev_PinMapping[DEV_MAX_MAPPING_NAME_STRLEN + 1];

#ifdef USE_DISPLAY_GRAPHIC
    Display_CONFIG_T Display;
#endif

    ZeroExport_CONFIG_T ZeroExport;

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    Led_Config_T Led[PINMAPPING_LED_COUNT];
#endif
};

class ConfigurationClass {
public:
    void init();
    bool read();
    bool write();
    void migrate();
    CONFIG_T& get();

    INVERTER_CONFIG_T* getFreeInverterSlot();
    INVERTER_CONFIG_T* getInverterConfig(uint64_t serial);
    void deleteInverterById(const uint8_t id);
};

extern ConfigurationClass Configuration;
