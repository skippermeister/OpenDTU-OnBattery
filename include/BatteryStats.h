// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "Arduino.h"
#include "AsyncJson.h"
#include "JkBmsDataPoints.h"
#include "VeDirectShuntController.h"

#pragma pack(push, 1)
typedef union {
    struct {
        unsigned overCurrentDischarge : 1;
        unsigned underTemperature : 1;
        unsigned overTemperature : 1;
        unsigned underVoltage : 1;
        unsigned overVoltage : 1;
        unsigned cellImbalance : 1;
        unsigned bmsInternal : 1;
        unsigned overCurrentCharge : 1;
    };
    uint8_t alarms;
} Alarm_t;

typedef union {
    struct {
        unsigned highCurrentDischarge : 1;
        unsigned lowTemperature : 1;
        unsigned highTemperature : 1;
        unsigned lowVoltage : 1;
        unsigned highVoltage : 1;
        unsigned cellImbalance : 1;
        unsigned bmsInternal : 1;
        unsigned highCurrentCharge;
    };
    uint8_t warnings;
} Warning_t;
#pragma pack(pop)

// mandatory interface for all kinds of batteries
class BatteryStats {
public:
    String const& getManufacturer() const { return manufacturer; }
    String const& getDeviceName() const { return deviceName; }

    // the last time *any* datum was updated
    uint32_t getAgeSeconds() const { return (millis() - lastUpdate) / 1000; }
    bool updateAvailable(uint32_t since) const { return lastUpdate > since; }

    uint8_t getSoC() const { return SoC; }

    virtual Alarm_t const& getAlarm() const
    {
        static Alarm_t dummy;
        return dummy;
    };
    virtual Warning_t const& getWarning() const
    {
        static Warning_t dummy;
        return dummy;
    };
    virtual bool getChargeEnabled() const { return true; };
    virtual bool getDischargeEnabled() const { return true; };
    virtual bool getChargeImmediately() const { return false; };

    uint32_t getSoCAgeSeconds() const { return (millis() - lastUpdateSoC) / 1000; }

    // convert stats to JSON for web application live view
    virtual void getLiveViewData(JsonVariant& root) const;

    virtual void mqttPublish(); // const;

    virtual float getVoltage() const { return 0.0; };

    virtual float getTemperature() const { return 0.0; };

    virtual float getRecommendedChargeVoltageLimit() const { return 0.0; };
    virtual float getRecommendedDischargeVoltageLimit() const { return 0.0; };
    virtual float getRecommendedChargeCurrentLimit() const { return 0.0; };
    virtual float getRecommendedDischargeCurrentLimit() const { return 0.0; };

    virtual float getMaximumChargeCurrentLimit() const { return 0.0; };
    virtual float getMaximumDischargeCurrentLimit() const { return 0.0; };

    virtual bool isChargeTemperatureValid() const { return false; };
    virtual bool isDischargeTemperatureValid() const { return false; };

    bool isValid() const { return lastUpdateSoC > 0 && lastUpdate > 0; }

    virtual bool initialized() const { return false; };

protected:
    template <typename T>
    void addLiveViewValue(JsonVariant& root, std::string const& name, T&& value, std::string const& unit, uint8_t precision) const;
    template <typename T>
    void addLiveViewParameter(JsonVariant& root, std::string const& name, T&& value, std::string const& unit, uint8_t precision) const;
    void addLiveViewText(JsonVariant& root, std::string const& name, std::string const& text) const;
    void addLiveViewWarning(JsonVariant& root, std::string const& name, bool warning) const;
    void addLiveViewAlarm(JsonVariant& root, std::string const& name, bool alarm) const;

    void addLiveViewCellVoltage(JsonVariant& root, uint8_t index, float value, uint8_t precision) const;
    void addLiveViewCellBalance(JsonVariant& root, std::string const& name, float value, std::string const& unit, uint8_t precision) const;
    void addLiveViewTempSensor(JsonVariant& root, uint8_t index, float value, uint8_t precision) const;

    String manufacturer = "unknown";
    String deviceName = "";
    uint8_t SoC = 0;
    uint32_t lastUpdateSoC = 0;
    uint32_t lastUpdate = 0;
};

/*
typedef struct {
    uint8_t NumberOfModules; // construct.Byte,
    struct { // NumberOfModules
        uint8_t NumberOfCells; // construct.Int8ub,
        float *CellVoltages; // pointer to array NumberOfCells ToVolt(construct.Int16sb)),
        uint8_t NumberOfTemperatures; // construct.Int8ub,
        float AverageBMSTemperature; // ToCelsius(construct.Int16sb),
        float *GroupedCellsTemperatures; // NumberOfTemperatures - 1, ToCelsius(construct.Int16sb)),
        float Current; // ToAmp(construct.Int16sb),
        float Voltage; // ToVolt(construct.Int16ub),
        float Power; // construct.Computed(construct.this.Current * construct.this.Voltage),
        float _RemainingCapacity1; // DivideBy1000(construct.Int16ub),
        uint8_t _UserDefinedItems; // Int8ub,
        float _TotalCapacity1; // DivideBy1000(Int16ub),
        int16_t CycleNumber; // construct.Int16ub,
        union {
            struct {
                // If(construct.this._UserDefinedItems > 2,
                float RemainingCapacity2; // DivideBy1000(Int24ub),
                float TotalCapacity2;  // DivideBy1000(Int24ub))),
            } _OptionalFields_t;
        };
        float RemainingCapacity; // if _UserDefinedItems > 2 then _OptionalFields.RemainingCapacity2 else this._RemainingCapacity1
        float TotalCapacity; // if _UserDefinedItems > 2_OptionalFields.TotalCapacity2 else this._TotalCapacity1
    } Module_t;
    float TotalPower; // Computed(lambda this: sum([x.Power for x in this.Module])),
    float StateOfCharge; // Computed(lambda this: sum([x.RemainingCapacity for x in this.Module]) / sum([x.TotalCapacity for x in this.Module])),
} Values_t;

typedef struct{
    uint8_t NumberOfModule; // Byte,
    uint8_t NumberOfCells; // Int8ub,
    float *CellVoltages; // pointer to Array( NumberOfCells, ToVolt(Int16sb)),
    uint8_t NumberOfTemperatures; // Int8ub
    float AverageBMSTemperature; // ToCelsius(Int16sb),
    float *GroupedCellsTemperatures; // pointer to Array(NumberOfTemperatures - 1, ToCelsius(Int16sb)),
    float Current; // ToAmp(Int16sb),
    float Voltage; // ToVolt(Int16ub),
    float Power; // compute Current * Voltage
    float _RemainingCapacity1; // DivideBy1000(Int16ub),
    uint8_t _UserDefinedItems; // Int8ub,
    float _TotalCapacity1; // DivideBy1000(Int16ub),
    int16_t CycleNumber; // Int16ub,
    union {
        // If(_UserDefinedItems > 2,
        struct {
            float RemainingCapacity2; // DivideBy1000(Int24ub),
            float TotalCapacity2; // DivideBy1000(Int24ub))),
        };
    } _OptionalFields;
    float RemainingCapacity; // if _UserDefinedItems > 2 then _OptionalFields.RemainingCapacity2 else _RemainingCapacity1
    float TotalCapacity; // if _UserDefinedItems > 2 then _OptionalFields.TotalCapacity2 else _TotalCapacity1
    float TotalPower; // Power
    float StateOfCharge; // RemainingCapacity / TotalCapacity),
} AnalogValue_t;
*/

typedef struct {
    char deviceName[11]; // JoinBytes(construct.Array(10, construct.Byte)),
    char softwareVersion[3]; // construct.Array(2, construct.Byte),
    char manufacturerName[21]; // JoinBytes(construct.GreedyRange(construct.Byte)),
} ManufacturerInfo_t;

typedef struct {
    float cellHighVoltageLimit; // ToVolt(construct.Int16ub),
    float cellLowVoltageLimit; // ToVolt(construct.Int16ub),
    float cellUnderVoltageLimit; // ToVolt(construct.Int16sb),
    float chargeHighTemperatureLimit; // ToCelsius(construct.Int16sb),
    float chargeLowTemperatureLimit; // ToCelsius(construct.Int16sb),
    float chargeCurrentLimit; // DivideBy100(construct.Int16sb),
    float moduleHighVoltageLimit; // ToVolt(construct.Int16ub),
    float moduleLowVoltageLimit; // ToVolt(construct.Int16ub),
    float moduleUnderVoltageLimit; // ToVolt(construct.Int16ub),
    float dischargeHighTemperatureLimit; // ToCelsius(construct.Int16sb),
    float dischargeLowTemperatureLimit; // ToCelsius(construct.Int16sb),
    float dischargeCurrentLimit; // DivideBy100(construct.Int16sb),
} SystemParameters_t;

typedef struct {
    uint8_t commandValue; // construct.Byte,
    uint8_t numberOfCells; // Int8ub,
    uint8_t* cellVoltages;
    uint8_t numberOfTemperatures; // Int8ub
    uint8_t BMSTemperature;
    uint8_t* Temperatures;
    uint8_t chargeCurrent;
    uint8_t moduleVoltage;
    uint8_t dischargeCurrent;
#pragma pack(push, 1)
    union {
        uint8_t status1;
        struct {
            unsigned moduleOverVoltage : 1;
            unsigned cellUnderVoltage : 1;
            unsigned chargeOverCurrent : 1;
            unsigned dummy : 1;
            unsigned dischargeOverCurrent : 1;
            unsigned dischargeOverTemperature : 1;
            unsigned chargeOverTemperature : 1;
            unsigned moduleUnderVoltage : 1;
        };
    };
#pragma pack(pop)
#pragma pack(push, 1)
    union {
        uint8_t status2;
        struct {
            unsigned preMOSFET : 1;
            unsigned chargeMOSFEET : 1;
            unsigned dischargeMOSFET : 1;
            unsigned usingBatteryModulePower : 1;
            unsigned dummy4 : 4;
        };
    };
#pragma pack(pop)
#pragma pack(push, 1)
    union {
        uint8_t status3;
        struct {
            unsigned buzzer : 1;
            unsigned dummy3 : 2;
            unsigned fullyCharged : 1;
            unsigned dummy1 : 1;
            unsigned reserve : 1;
            unsigned effectiveDischargeCurrent : 1;
            unsigned effectiveChargeCurrent : 1;
        };
    };
#pragma pack(pop)
#pragma pack(push, 1)
    union {
        struct {
            uint8_t status4;
            uint8_t status5;
        };
        uint16_t cellError;
    };
#pragma pack(pop)
} AlarmInfo_t;

typedef struct {
    uint8_t commandValue; // construct.Byte,
    float chargeVoltageLimit; // construct.Array(2, construct.Byte),
    float dischargeVoltageLimit; // construct.Array(2, construct.Byte),
    float chargeCurrentLimit; // construct.Array(2, construct.Byte),
    float dischargeCurrentLimit; // construct.Array(2, construct.Byte),
#pragma pack(push, 1)
    union {
        uint8_t Status; // construct.Byte,
        struct {
            unsigned dummy : 3;
            unsigned fullChargeRequest : 1; // if SOC never higher than 97% in 30 days, will set this flag to 1. And when the SOC is ≥ 97%, the flag will be 0.
            unsigned chargeImmediately : 1; // 9-13%
            unsigned chargeImmediately1 : 1; // 5-9%
            unsigned dischargeEnable : 1; // 1: yes；0: request stop discharge
            unsigned chargeEnable : 1; // 1: yes；0: request stop charge
        };
    };
#pragma pack(pop)
} ChargeDischargeManagementInfo_t;

typedef struct {
    uint8_t commandValue; // construct.Byte,
    uint8_t moduleSerialNumber[17]; // JoinBytes(construct.Array(16, construct.Byte))
} ModuleSerialNumber_t;

#ifdef USE_PYLONTECH_CAN_RECEIVER
class PylontechCanBatteryStats : public BatteryStats {
    friend class PylontechCanReceiver;

public:
    void getLiveViewData(JsonVariant& root) const final;
    const Alarm_t& getAlarm() const final { return Alarm; };
    const Warning_t& getWarning() const final { return Warning; };
    bool getChargeEnabled() const final { return chargeEnabled; };
    bool getDischargeEnabled() const final { return dischargeEnabled; };
    bool getChargeImmediately() const final { return chargeImmediately; };

    float getVoltage() const final { return voltage; };

    bool isChargeTemperatureValid() const final { return true; }; // FIXME: to be done
    bool isDischargeTemperatureValid() const final { return true; }; // FIXME: to be done

    void mqttPublish() final; // const final;

    bool initialized() const final { return _initialized; };

private:
    void setManufacturer(String&& m) { manufacturer = std::move(m) + "tech"; }
    void setDeviceName(String&& m) { deviceName = std::move(m); }
    void setSoC(uint8_t value)
    {
        SoC = value;
        lastUpdateSoC = millis();
    }
    void setLastUpdate(uint32_t ts) { lastUpdate = ts; }

    float chargeVoltage;
    float chargeCurrentLimit;
    float dischargeCurrentLimit;
    float dischargeVoltage;

    uint16_t stateOfHealth;
    float voltage; // total voltage of the battery pack
    // total current into (positive) or from (negative)
    // the battery, i.e., the charging current
    float current;
    float temperature;
    float power;

    Alarm_t Alarm;
    Warning_t Warning;

    bool chargeEnabled;
    bool dischargeEnabled;
    bool chargeImmediately;
    bool chargeImmediately1;
    bool fullChargeRequest;

    struct {
        float chargeVoltage;
        float chargeCurrentLimit;
        float dischargeCurrentLimit;
        float dischargeVoltage;

        uint16_t stateOfHealth;
        float voltage; // total voltage of the battery pack
        // total current into (positive) or from (negative)
        // the battery, i.e., the charging current
        float current;
        float temperature;
        float power;

        Alarm_t Alarm;
        Warning_t Warning;

        union {
            uint8_t Status;
            struct {
                unsigned chargeEnabled : 1;
                unsigned dischargeEnabled : 1;
                unsigned chargeImmediately : 1;
                unsigned chargeImmediately1 : 1;
                unsigned fullChargeRequest : 1;
            };
        };
    } _last;

    bool _initialized = false;
};
#endif

class PylontechRS485BatteryStats : public BatteryStats {
    friend class PylontechRS485Receiver;

public:
    void getLiveViewData(JsonVariant& root) const final;

    const Alarm_t& getAlarm() const final { return Alarm; };
    const Warning_t& getWarning() const final { return Warning; };
    bool getChargeEnabled() const final { return ChargeDischargeManagementInfo.chargeEnable; };
    bool getDischargeEnabled() const final { return ChargeDischargeManagementInfo.dischargeEnable; };
    bool getChargeImmediately() const final
    {
        if (ChargeDischargeManagementInfo.chargeImmediately || ChargeDischargeManagementInfo.chargeImmediately1 || ChargeDischargeManagementInfo.fullChargeRequest) {
            return true;
        }
        return false;
    };
    float getVoltage() const final { return voltage; };

    float getTemperature() const final { return averageCellTemperature; };

    float getRecommendedChargeVoltageLimit() const final { return ChargeDischargeManagementInfo.chargeVoltageLimit; };
    float getRecommendedDischargeVoltageLimit() const final { return ChargeDischargeManagementInfo.dischargeVoltageLimit; };
    float getRecommendedChargeCurrentLimit() const final { return ChargeDischargeManagementInfo.chargeCurrentLimit; };
    float getRecommendedDischargeCurrentLimit() const final { return ChargeDischargeManagementInfo.dischargeCurrentLimit; };
    float getMaximumChargeCurrentLimit() const final { return SystemParameters.chargeCurrentLimit; };
    float getMaximumDischargeCurrentLimit() const final { return SystemParameters.dischargeCurrentLimit; };

    bool isChargeTemperatureValid() const final;
    bool isDischargeTemperatureValid() const final;

    void mqttPublish() final; // const final;

    bool initialized() const final { return _initialized; };

private:
    void setManufacturer(String&& m) { manufacturer = std::move(m) + "tech"; }
    void setDeviceName(String&& m) { deviceName = std::move(m); }
    void setSoC(uint8_t value)
    {
        SoC = value;
        lastUpdateSoC = millis();
    }
    void setLastUpdate(uint32_t ts) { lastUpdate = ts; }

    String softwareVersion;
    String manufacturerVersion;
    String mainLineVersion;

    float chargeVoltage;
    float voltage; // total voltage of the battery pack
    // total current into (positive) or from (negative) the battery, i.e., the charging current
    float current;

    SystemParameters_t SystemParameters;
    Alarm_t Alarm;
    Warning_t Warning;

    float power;
    float totalPower;
    float totalCapacity;
    float remainingCapacity;
    uint16_t cycles;

    uint8_t numberOfModule;
    float cellMinVoltage;
    float cellMaxVoltage;
    float cellDiffVoltage;
    uint8_t numberOfCells;
    float* CellVoltages;
    uint8_t numberOfTemperatures;
    float averageBMSTemperature;
    float* GroupedCellsTemperatures;
    float averageCellTemperature;
    float minCellTemperature;
    float maxCellTemperature;

    struct {
        String softwareVersion;

        float chargeVoltage;
        float voltage; // total voltage of the battery pack
        // total current into (positive) or from (negative) the battery, i.e., the charging current
        float current;

        SystemParameters_t SystemParameters;

        Alarm_t Alarm;
        Warning_t Warning;

        float power;
        float totalPower;
        float totalCapacity;
        float remainingCapacity;
        uint16_t cycles;

        uint8_t numberOfModule;
        float cellMinVoltage;
        float cellMaxVoltage;
        float cellDiffVoltage;
        uint8_t numberOfCells;
        float* CellVoltages;
        uint8_t numberOfTemperatures;
        float averageBMSTemperature;
        float* GroupedCellsTemperatures;
        float averageCellTemperature;

        ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
    } _last;

    AlarmInfo_t AlarmInfo;
    ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
    ModuleSerialNumber_t ModuleSerialNumber;

    bool _initialized = false;
};

#ifdef USE_JKBMS_CONTROLLER
class JkBmsBatteryStats : public BatteryStats {
public:
    void getLiveViewData(JsonVariant& root) const final;

    bool isChargeTemperatureValid() const final { return true; }; // FIXME: to be done
    bool isDischargeTemperatureValid() const final { return true; }; // FIXME: to be done

    void mqttPublish() final; // const final;

    void updateFrom(JkBms::DataPointContainer const& dp);

    bool initialized() const final { return _initialized; };

private:
    JkBms::DataPointContainer _dataPoints;
    mutable uint32_t _lastMqttPublish = 0;
    mutable uint32_t _lastFullMqttPublish = 0;

    bool _initialized = false;
};
#endif

#ifdef USE_DALYBMS_CONTROLLER
class DalyBmsBatteryStats : public BatteryStats {
public:
    void getLiveViewData(JsonVariant& root) const final;

    bool isChargeTemperatureValid() const final { return true; }; // FIXME: to be done
    bool isDischargeTemperatureValid() const final { return true; }; // FIXME: to be done

    void mqttPublish() final; // const final;

    void updateFrom(DalyBms::DataPointContainer const& dp);

    bool initialized() const final { return _initialized; };

private:
    DalyBms::DataPointContainer _dataPoints;
    mutable uint32_t _lastMqttPublish = 0;
    mutable uint32_t _lastFullMqttPublish = 0;

    bool _initialized = false;
};
#endif

#ifdef USE_VICTRON_SMART_SHUNT
class VictronSmartShuntStats : public BatteryStats {
public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() final;

    void updateFrom(VeDirectShuntController::veShuntStruct const& shuntData);

    bool initialized() const final { return _initialized; };

private:
    float _voltage;
    float _current;
    float _temperature;
    bool _tempPresent;
    uint8_t _chargeCycles;
    uint32_t _timeToGo;
    float _chargedEnergy;
    float _dischargedEnergy;
    String _modelName;

    bool _alarmLowVoltage;
    bool _alarmHighVoltage;
    bool _alarmLowSOC;
    bool _alarmLowTemperature;
    bool _alarmHighTemperature;

    bool _initialized = false;
};
#endif

#ifdef USE_MQTT_BATTERY
class MqttBatteryStats : public BatteryStats {
    public:
        // since the source of information was MQTT in the first place,
        // we do NOT publish the same data under a different topic.
        void mqttPublish() const final { }

        // the SoC is the only interesting value in this case, which is already
        // displayed at the top of the live view. do not generate a card.
        void getLiveViewData(JsonVariant& root) const final { }

        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = _lastUpdate = millis(); }
};
#endif
