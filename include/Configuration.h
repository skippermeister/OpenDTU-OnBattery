// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "PinMapping.h"
#include <cstdint>
#include <ArduinoJson.h>

#define CONFIG_FILENAME "/config.json"
#define CONFIG_VERSION 0x00011c00 // 0.1.28 // make sure to clean all after change

#define WIFI_MAX_SSID_STRLEN 32
#define WIFI_MAX_PASSWORD_STRLEN 64
#define WIFI_MAX_HOSTNAME_STRLEN 31

#define NTP_MAX_SERVER_STRLEN 31
#define NTP_MAX_TIMEZONE_STRLEN 50
#define NTP_MAX_TIMEZONEDESCR_STRLEN 50

#define MQTT_MAX_HOSTNAME_STRLEN 128
#define MQTT_MAX_CLIENTID_STRLEN 64
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

#define HTTP_REQUEST_MAX_URL_STRLEN 256 // 1024
#define HTTP_REQUEST_MAX_USERNAME_STRLEN 64
#define HTTP_REQUEST_MAX_PASSWORD_STRLEN 64
#define HTTP_REQUEST_MAX_HEADER_KEY_STRLEN 64
#define HTTP_REQUEST_MAX_HEADER_VALUE_STRLEN 256

#define POWERMETER_MQTT_MAX_VALUES 3
#define POWERMETER_HTTP_JSON_MAX_VALUES 3
#define POWERMETER_HTTP_JSON_MAX_PATH_STRLEN 256
#define BATTERY_JSON_MAX_PATH_STRLEN 128

#ifdef USE_LED_SINGLE
    #define LED_COUNT   PINMAPPING_LED_COUNT
#endif
#ifdef USE_LED_STRIP
    #define LED_COUNT   2
#endif

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
    bool ClearEventlogOnMidnight;
    bool YieldDayCorrection;
    CHANNEL_CONFIG_T channel[INV_MAX_CHAN_COUNT];
};

typedef struct {
    uint64_t Serial;
    uint32_t PollInterval;
#ifdef USE_RADIO_NRF
    struct {
        int8_t PaLevel;
    } Nrf;
#endif
#ifdef USE_RADIO_CMT
    struct {
        int8_t PaLevel;
        uint32_t Frequency;
        uint8_t CountryMode;
    } Cmt;
#endif
} Dtu_CONFIG_T;

struct HTTP_REQUEST_CONFIG_T {
    char Url[HTTP_REQUEST_MAX_URL_STRLEN + 1];

    enum Auth { None, Basic, Digest };
    Auth AuthType;

    char Username[HTTP_REQUEST_MAX_USERNAME_STRLEN + 1];
    char Password[HTTP_REQUEST_MAX_PASSWORD_STRLEN + 1];
    char HeaderKey[HTTP_REQUEST_MAX_HEADER_KEY_STRLEN + 1];
    char HeaderValue[HTTP_REQUEST_MAX_HEADER_VALUE_STRLEN + 1];
    uint16_t Timeout;
};
using HttpRequestConfig = struct HTTP_REQUEST_CONFIG_T;

struct POWERMETER_MQTT_VALUE_T {
    char Topic[MQTT_MAX_TOPIC_STRLEN + 1];
    char JsonPath[POWERMETER_HTTP_JSON_MAX_PATH_STRLEN + 1];

    enum Unit { Watts = 0, MilliWatts = 1, KiloWatts = 2 };
    Unit PowerUnit;

    bool SignInverted;
};
using PowerMeterMqttValue = struct POWERMETER_MQTT_VALUE_T;

struct POWERMETER_MQTT_CONFIG_T {
    PowerMeterMqttValue Values[POWERMETER_MQTT_MAX_VALUES];
};
using PowerMeterMqttConfig = struct POWERMETER_MQTT_CONFIG_T;

struct POWERMETER_SERIAL_SDM_CONFIG_T {
    uint32_t Baudrate;
    uint32_t Address;
    uint32_t PollingInterval;
};
using PowerMeterSerialSdmConfig = struct POWERMETER_SERIAL_SDM_CONFIG_T;

struct POWERMETER_HTTP_JSON_VALUE_T {
    HttpRequestConfig HttpRequest;
    bool Enabled;
    char JsonPath[POWERMETER_HTTP_JSON_MAX_PATH_STRLEN + 1];

    enum Unit { Watts = 0, MilliWatts = 1, KiloWatts = 2 };
    Unit PowerUnit;

    bool SignInverted;
};
using PowerMeterHttpJsonValue = struct POWERMETER_HTTP_JSON_VALUE_T;

struct POWERMETER_HTTP_JSON_CONFIG_T {
    uint32_t PollingInterval;
    bool IndividualRequests;
    PowerMeterHttpJsonValue Values[POWERMETER_HTTP_JSON_MAX_VALUES];
};
using PowerMeterHttpJsonConfig = struct POWERMETER_HTTP_JSON_CONFIG_T;

struct POWERMETER_HTTP_SML_CONFIG_T {
    uint32_t PollingInterval;
    HttpRequestConfig HttpRequest;
};
using PowerMeterHttpSmlConfig = struct POWERMETER_HTTP_SML_CONFIG_T;

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
        bool modbus_tcp_enabled;
        bool modbus_delaystart;
        char mfrname[32];
        char modelname[32];
        char options[16];
        char version[16];
        char serial[16];
};

struct Ntp_CONFIG_T {
    char Server[NTP_MAX_SERVER_STRLEN + 1];
    char Timezone[NTP_MAX_TIMEZONE_STRLEN + 1];
    char TimezoneDescr[NTP_MAX_TIMEZONEDESCR_STRLEN + 1];
    double Longitude;
    double Latitude;
    uint8_t SunsetType;
    float Sunrise;
    float Sunset;
};

struct PowerMeter_CONFIG_T {
    bool Enabled;
    bool VerboseLogging;
    bool UpdatesOnly;
    uint32_t Source;
    PowerMeterMqttConfig Mqtt;
    PowerMeterSerialSdmConfig SerialSdm;
    PowerMeterHttpJsonConfig HttpJson;
    PowerMeterHttpSmlConfig HttpSml;
};

struct PowerLimiter_CONFIG_T {
    bool Enabled;
    bool VerboseLogging;
    bool SolarPassThroughEnabled;
    uint8_t SolarPassThroughLosses;
    bool BatteryAlwaysUseAtNight;
    bool UpdatesOnly;
    bool IsInverterBehindPowerMeter;
    bool IsInverterSolarPowered;
    bool UseOverscalingToCompensateShading;
    uint64_t InverterId;
    uint8_t InverterChannelId;
    int32_t TargetPowerConsumption;
    int32_t TargetPowerConsumptionHysteresis;
    int32_t LowerPowerLimit;
    int32_t BaseLoadLimit;
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

enum BatteryVoltageUnit { Volts = 0, DeciVolts = 1, CentiVolts = 2, MilliVolts = 3 };

struct Battery_CONFIG_T {
    bool Enabled;
    bool VerboseLogging;
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
        char SocJsonPath[BATTERY_JSON_MAX_PATH_STRLEN + 1];
        char VoltageTopic[MQTT_MAX_TOPIC_STRLEN + 1];
        char VoltageJsonPath[BATTERY_JSON_MAX_PATH_STRLEN + 1];
        BatteryVoltageUnit VoltageUnit;
    } Mqtt;
#endif
#if defined(USE_MQTT_BATTERY) || defined(USE_VICTRON_SMART_SHUNT)
    float RecommendedChargeVoltage;
    float RecommendedDischargeVoltage;
#endif

    bool UpdatesOnly;
    int8_t MinChargeTemperature;
    int8_t MaxChargeTemperature;
    int8_t MinDischargeTemperature;
    int8_t MaxDischargeTemperature;
    uint8_t Stop_Charging_BatterySoC_Threshold;
};

struct Mqtt_CONFIG_T {
    bool Enabled;
    char Hostname[MQTT_MAX_HOSTNAME_STRLEN + 1];
    uint32_t Port;
    char ClientId[MQTT_MAX_CLIENTID_STRLEN + 1];
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

#ifdef USE_CHARGER_HUAWEI
struct Huawei_CONFIG_T {
    bool Enabled;
    bool VerboseLogging;
    bool Auto_Power_Enabled;
    bool Auto_Power_BatterySoC_Limits_Enabled;
    bool Emergency_Charge_Enabled;
    float Auto_Power_Voltage_Limit;
    float Auto_Power_Enable_Voltage_Limit;
    float Auto_Power_Lower_Power_Limit;
    float Auto_Power_Upper_Power_Limit;
    uint8_t Auto_Power_Stop_BatterySoC_Threshold;
    float Auto_Power_Target_Power_Consumption;
};
#endif

#ifdef USE_CHARGER_MEANWELL
struct MeanWell_CONFIG_T {
    bool Enabled;
    bool VerboseLogging;
    bool UpdatesOnly;
    uint32_t PollInterval;
    float MinVoltage;
    float MaxVoltage;
    float MinCurrent;
    float MaxCurrent;
    float VoltageLimitMin;
    float VoltageLimitMax;
    float CurrentLimitMin;
    float CurrentLimitMax;
    float Hysteresis;
    bool mustInverterProduce;
};
#endif

struct Vedirect_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
};

#ifdef USE_REFUsol_INVERTER
struct REFUsol_CONFIG_T {
    bool Enabled;
    bool UpdatesOnly;
    uint32_t PollInterval;
};
#endif

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
    Mqtt_CONFIG_T Mqtt;
    Dtu_CONFIG_T Dtu;

    struct {
        char Password[WIFI_MAX_PASSWORD_STRLEN + 1];
        bool AllowReadonly;
    } Security;

#ifdef USE_DISPLAY_GRAPHIC
    Display_CONFIG_T Display;
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    Led_Config_T Led[PINMAPPING_LED_COUNT];
#endif

    Vedirect_CONFIG_T Vedirect;
    PowerMeter_CONFIG_T PowerMeter;
    PowerLimiter_CONFIG_T PowerLimiter;
    Battery_CONFIG_T Battery;

    struct {
        uint32_t Controller_Frequency;
    } MCP2515;

#ifdef USE_CHARGER_HUAWEI
    Huawei_CONFIG_T Huawei;
#endif
#ifdef USE_CHARGER_MEANWELL
    MeanWell_CONFIG_T MeanWell;
#endif

#ifdef USE_REFUsol_INVERTER
    REFUsol_CONFIG_T REFUsol;
#endif

    INVERTER_CONFIG_T Inverter[INV_MAX_COUNT];
    char Dev_PinMapping[DEV_MAX_MAPPING_NAME_STRLEN + 1];

    ZeroExport_CONFIG_T ZeroExport;
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

    static void serializeHttpRequestConfig(HttpRequestConfig const& source, JsonObject& target);
    static void serializePowerMeterMqttConfig(PowerMeterMqttConfig const& source, JsonObject& target);
    static void serializePowerMeterSerialSdmConfig(PowerMeterSerialSdmConfig const& source, JsonObject& target);
    static void serializePowerMeterHttpJsonConfig(PowerMeterHttpJsonConfig const& source, JsonObject& target);
    static void serializePowerMeterHttpSmlConfig(PowerMeterHttpSmlConfig const& source, JsonObject& target);

    static void deserializeHttpRequestConfig(JsonObject const& source, HttpRequestConfig& target);
    static void deserializePowerMeterMqttConfig(JsonObject const& source, PowerMeterMqttConfig& target);
    static void deserializePowerMeterSerialSdmConfig(JsonObject const& source, PowerMeterSerialSdmConfig& target);
    static void deserializePowerMeterHttpJsonConfig(JsonObject const& source, PowerMeterHttpJsonConfig& target);
    static void deserializePowerMeterHttpSmlConfig(JsonObject const& source, PowerMeterHttpSmlConfig& target);
};

extern ConfigurationClass Configuration;
