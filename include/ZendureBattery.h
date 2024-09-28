#ifdef USE_MQTT_ZENDURE_BATTERY
#pragma once

#include <optional>
#include "Battery.h"
#include <espMqttClient.h>

// DeviceIDs for compatible Solarflow devices
#define ZENDURE_HUB1200     "73bkTV"
#define ZENDURE_HUB2000     "A8yh63"
#define ZENDURE_AIO2400     "yWF7hV)"
#define ZENDURE_ACE1500     "8bM93H"
#define ZENDURE_HYPER2000   "ja72U0ha)"

#define ZENDURE_MAX_PACKS                           4U
#define ZENDURE_REMAINING_TIME_OVERFLOW             59940U

#define ZENDURE_LOG_OFFSET_SOC                      0               // [%]
#define ZENDURE_LOG_OFFSET_PACKNUM                  1               // [1]
#define ZENDURE_LOG_OFFSET_PACK_SOC(pack)           (2+(pack)-1)    // [d%]
#define ZENDURE_LOG_OFFSET_PACK_VOLTAGE(pack)       (6+(pack)-1)    // [cV]
#define ZENDURE_LOG_OFFSET_PACK_CURRENT(pack)       (10+(pack)-1)   // [dA]
#define ZENDURE_LOG_OFFSET_PACK_CELL_MIN(pack)      (14+(pack)-1)   // [cV]
#define ZENDURE_LOG_OFFSET_PACK_CELL_MAX(pack)      (18+(pack)-1)   // [cV]
#define ZENDURE_LOG_OFFSET_PACK_UNKOWN_1(pack)      (22+(pack)-1)   // ? => always (0 | 0 | 0 |0)
#define ZENDURE_LOG_OFFSET_PACK_UNKOWN_2(pack)      (26+(pack)-1)   // ? => always (0 | 0 | 0 |0)
#define ZENDURE_LOG_OFFSET_PACK_UNKOWN_3(pack)      (30+(pack)-1)   // ? => always (8449 | 257 | 0 | 0)
#define ZENDURE_LOG_OFFSET_PACK_TEMPERATURE(pack)   (34+(pack)-1)   // [°C]
#define ZENDURE_LOG_OFFSET_PACK_UNKOWN_5(pack)      (38+(pack)-1)   // ? => always (1340 | 99 | 0 | 0)
#define ZENDURE_LOG_OFFSET_VOLTAGE                  42              // [dV]
#define ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_2       43              // [W]
#define ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_1       44              // [W]
#define ZENDURE_LOG_OFFSET_OUTPUT_POWER             45              // [W]
#define ZENDURE_LOG_OFFSET_UNKOWN_05                46              // ? => 1, 413
#define ZENDURE_LOG_OFFSET_DISCHARGE_POWER          47              // [W]
#define ZENDURE_LOG_OFFSET_CHARGE_POWER             48              // [W]
#define ZENDURE_LOG_OFFSET_OUTPUT_POWER_LIMIT       49              // [cA]
#define ZENDURE_LOG_OFFSET_UNKOWN_08                50              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_09                51              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_10                52              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_11                53              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_12                54              // ? => always 0
#define ZENDURE_LOG_OFFSET_BYPASS_MODE              55              // [0=Auto | 1=AlwaysOff | 2=AlwaysOn]
#define ZENDURE_LOG_OFFSET_UNKOWN_14                56              // ? => always 3
#define ZENDURE_LOG_OFFSET_UNKOWN_15                57              // ? Some kind of bitmask => e.g. 813969441
#define ZENDURE_LOG_OFFSET_UNKOWN_16                58              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_17                59              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_18                60              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_19                61              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_20                62              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_21                63              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_22                64              // ? => always 1
#define ZENDURE_LOG_OFFSET_UNKOWN_23                65              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_24                66              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_25                67              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_26                68              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_27                69              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_28                70              // ? => always 1
#define ZENDURE_LOG_OFFSET_UNKOWN_29                71              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_30                72              // ? some counter => 258, 263, 25
#define ZENDURE_LOG_OFFSET_UNKOWN_31                73              // ? some counter => 309, 293, 23
#define ZENDURE_LOG_OFFSET_UNKOWN_32                74              // ? => always 1
#define ZENDURE_LOG_OFFSET_UNKOWN_33                75              // ? => always 1
#define ZENDURE_LOG_OFFSET_UNKOWN_34                76              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_35                77              // ? => always 0 or 1
#define ZENDURE_LOG_OFFSET_UNKOWN_36                78              // ? => always 0 or 1
#define ZENDURE_LOG_OFFSET_UNKOWN_37                79              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_38                80              // ? => always 1 or 0
#define ZENDURE_LOG_OFFSET_AUTO_RECOVER             81              // [bool]
#define ZENDURE_LOG_OFFSET_UNKOWN_40                82              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_41                83              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_42                84              // ? => always 0
#define ZENDURE_LOG_OFFSET_MIN_SOC                  85              // [%]
#define ZENDURE_LOG_OFFSET_UNKOWN_44                86              // State 0 == Idle | 1 == Discharge
#define ZENDURE_LOG_OFFSET_UNKOWN_45                87              // ? => always 512
#define ZENDURE_LOG_OFFSET_UNKOWN_46                88              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_47                89              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_48                90              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_49                91              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_50                92              // ? => always 0
#define ZENDURE_LOG_OFFSET_UNKOWN_51                93              // ? => always 0

#define ZENDURE_REPORT_PACK_DATE                    "packData"
#define ZENDURE_REPORT_PROPERTIES                   "properties"
#define ZENDURE_REPORT_MIN_SOC                      "minSoc"
#define ZENDURE_REPORT_MAX_SOC                      "socSet"
#define ZENDURE_REPORT_INPUT_LIMIT                  "inputLimit"
#define ZENDURE_REPORT_OUTPUT_LIMIT                 "outputLimit"
#define ZENDURE_REPORT_INVERSE_MAX_POWER            "inverseMaxPower"
#define ZENDURE_REPORT_PACK_STATE                   "packState"
#define ZENDURE_REPORT_HEAT_STATE                   "heatState"
#define ZENDURE_REPORT_AUTO_SHUTDOWN                "hubState"
#define ZENDURE_REPORT_BUZZER_SWITCH                "buzzerSwitch"
#define ZENDURE_REPORT_REMAIN_OUT_TIME              "remainOutTime"
#define ZENDURE_REPORT_REMAIN_IN_TIME               "remainInputTime"
#define ZENDURE_REPORT_FW_VERSION                   "softVersion"
#define ZENDURE_REPORT_HW_VERSION                   "masterhaerVersion"
#define ZENDURE_REPORT_SERIAL                       "sn"
#define ZENDURE_REPORT_STATE                        "state"
#define ZENDURE_REPORT_AUTO_RECOVER                 "autoRecover"
#define ZENDURE_REPORT_BYPASS_STATE                 "pass"
#define ZENDURE_REPORT_BYPASS_MODE                  "passMode"
#define ZENDURE_REPORT_PV_BRAND                     "pvBrand"
#define ZENDURE_REPORT_SMART_MODE                   "smartMode"
#define ZENDURE_REPORT_PV_AUTO_MODEL                "autoModel"


#define ZENDURE_NO_REDUCED_UPDATE

class ZendureBattery : public BatteryProvider {
public:
    ZendureBattery() = default;

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    uint16_t setOutputLimit(uint16_t limit);

protected:
    void timesync();
    static String parseVersion(uint32_t version);

private:
    void calculateEfficiency();

    bool _verboseLogging = false;

#ifndef ZENDURE_NO_REDUCED_UPDATE
    uint32_t _rateUpdateMs;
    uint64_t _nextUpdate;
#endif

    uint32_t _rateFullUpdateMs;
    uint64_t _nextFullUpdate;

    uint32_t _rateTimesyncMs;
    uint64_t _nextTimesync;

    String _deviceId;

    String _baseTopic;
    String _topicLog;
    String _topicReadReply;
    String _topicReport;
    String _topicRead;
    String _topicWrite;
    String _topicTimesync;

    String _payloadSettings;
    String _payloadFullUpdate;
#ifndef ZENDURE_NO_REDUCED_UPDATE
    String _payloadUpdate;
#endif


    std::shared_ptr<ZendureBatteryStats> _stats = std::make_shared<ZendureBatteryStats>();

    void onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

    void onMqttMessageLog(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

#ifndef ZENDURE_NO_REDUCED_UPDATE
    void onMqttMessageRead(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);
#endif
};
#endif
