// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "Arduino.h"
#include "AsyncJson.h"
#include "JkBmsDataPoints.h"
#include "VeDirectShuntController.h"
#include "Configuration.h"

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
        String const& getManufacturer() const { return _manufacturer; }
        String const& getDeviceName() const { return _deviceName; }

        // the last time *any* datum was updated
        uint32_t getAgeSeconds() const { return (millis() - _lastUpdate) / 1000; }
        bool updateAvailable(uint32_t since) const { return _lastUpdate > since; }

        uint8_t getSoC() const { return _SoC; }
        uint32_t getSoCAgeSeconds() const { return (millis() - _lastUpdateSoC) / 1000; }

        // convert stats to JSON for web application live view
        virtual void getLiveViewData(JsonVariant& root) const;

        void mqttLoop();

        // the interval at which all battery datums will be re-published, even
        // if they did not change. used to calculate Home Assistent expiration.
        virtual uint32_t getMqttFullPublishIntervalMs() const;

        virtual Alarm_t const& getAlarm() const { static Alarm_t dummy; return dummy; };
        virtual Warning_t const& getWarning() const { static Warning_t dummy; return dummy; };
        virtual bool getChargeEnabled() const { return true; };
        virtual bool getDischargeEnabled() const { return true; };
        virtual bool getChargeImmediately() const { return false; };
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

        bool isValid() const { return _lastUpdateSoC > 0 && _lastUpdate > 0; }

        virtual bool initialized() const { return false; };

    protected:
        virtual void mqttPublish() /*const*/;

        String _manufacturer = "unknown";
        String _deviceName = "";
        uint8_t _SoC = 0;
        uint32_t _lastUpdateSoC = 0;
        uint32_t _lastUpdate = 0;

    private:
        uint32_t _lastMqttPublish = 0;
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

    void mqttPublish() /*const*/ final;

    uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

    bool initialized() const final { return _initialized; };

private:
    void setManufacturer(String&& m) { _manufacturer = std::move(m) + "tech"; }
    void setDeviceName(String&& m) { _deviceName = std::move(m); }
    void setSoC(uint8_t value) { _SoC = value; _lastUpdateSoC = millis(); }
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

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

#ifdef USE_PYLONTECH_RS485_RECEIVER
class PylontechRS485BatteryStats : public BatteryStats {
    friend class PylontechRS485Receiver;

public:
    void getLiveViewData(JsonVariant& root) const final;

    const Alarm_t& getAlarm() const final { return Alarm; };
    const Warning_t& getWarning() const final { return Warning; };
    bool getChargeEnabled() const final { return ChargeDischargeManagementInfo.chargeEnable; };
    bool getDischargeEnabled() const final { return ChargeDischargeManagementInfo.dischargeEnable; };
    bool getChargeImmediately() const final { return (ChargeDischargeManagementInfo.chargeImmediately || ChargeDischargeManagementInfo.chargeImmediately1 || ChargeDischargeManagementInfo.fullChargeRequest); };
    float getVoltage() const final { return voltage; };
    float getTemperature() const final { return averageCellTemperature; };

    float getRecommendedChargeVoltageLimit() const final { return ChargeDischargeManagementInfo.chargeVoltageLimit; };
    float getRecommendedDischargeVoltageLimit() const final { return ChargeDischargeManagementInfo.dischargeVoltageLimit; };
    float getRecommendedChargeCurrentLimit() const final { return ChargeDischargeManagementInfo.chargeCurrentLimit; };
    float getRecommendedDischargeCurrentLimit() const final { return ChargeDischargeManagementInfo.dischargeCurrentLimit; };
    float getMaximumChargeCurrentLimit() const final { return SystemParameters.chargeCurrentLimit; };
    float getMaximumDischargeCurrentLimit() const final { return SystemParameters.dischargeCurrentLimit; };

    bool isChargeTemperatureValid() const final
    {
        const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
        return (minCellTemperature >= max(SystemParameters.chargeLowTemperatureLimit, static_cast<float>(cBattery.MinChargeTemperature)))
            && (maxCellTemperature <= min(SystemParameters.chargeHighTemperatureLimit, static_cast<float>(cBattery.MaxChargeTemperature)));
    };
    bool isDischargeTemperatureValid() const final
    {
        const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
        return (minCellTemperature >= max(SystemParameters.dischargeLowTemperatureLimit, static_cast<float>(cBattery.MinDischargeTemperature)))
            && (maxCellTemperature <= min(SystemParameters.dischargeHighTemperatureLimit, static_cast<float>(cBattery.MaxDischargeTemperature)));
    };

    void mqttPublish() /*const*/ final;

    uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

    bool initialized() const final { return _initialized; };

private:
    void setManufacturer(String&& m) { _manufacturer = std::move(m) + "tech"; }
    void setDeviceName(String&& m) { _deviceName = std::move(m); }
    void setSoC(uint8_t value) { _SoC = value; _lastUpdateSoC = millis(); }
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    String softwareVersion;
    String _manufacturerVersion;
    String _mainLineVersion;

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
#endif

#ifdef USE_JKBMS_CONTROLLER
class JkBmsBatteryStats : public BatteryStats {
public:
    void getLiveViewData(JsonVariant& root) const final;

    bool isChargeTemperatureValid() const final { return true; }; // FIXME: to be done
    bool isDischargeTemperatureValid() const final { return true; }; // FIXME: to be done

    void mqttPublish() /*const*/ final;

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
    friend class DalyBmsController;

    public:
        void getLiveViewData(JsonVariant& root) const final;

        const Alarm_t& getAlarm() const final { return Alarm; };
        const Warning_t& getWarning() const final { return Warning; };
        bool getChargeEnabled() const final { return _chargingMosEnabled; };
        bool getDischargeEnabled() const final { return _dischargingMosEnabled; };
        bool getChargeImmediately() const final { return (_batteryLevel < AlarmValues.minSoc); };
        float getVoltage() const final { return _voltage; };
        float getTemperature() const final { return (_maxTemperature + _minTemperature) / 2.0; };

        float getRecommendedChargeVoltageLimit() const final { return WarningValues.maxPackVoltage * 0.99; };  // 1% below warning level
        float getRecommendedDischargeVoltageLimit() const final { return WarningValues.minPackVoltage * 1.01; }; // 1% above warning level
        float getRecommendedChargeCurrentLimit() const final { return WarningValues.maxPackChargeCurrent * 0.9; }; // 10% below warning level
        float getRecommendedDischargeCurrentLimit() const final { return WarningValues.maxPackDischargeCurrent * 0.9; }; // 10% below warning level
        float getMaximumChargeCurrentLimit() const final { return WarningValues.maxPackChargeCurrent; };
        float getMaximumDischargeCurrentLimit() const final { return WarningValues.maxPackDischargeCurrent; };

        bool isChargeTemperatureValid() const final
        {
            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_minTemperature >= cBattery.MinChargeTemperature) && (_maxTemperature <= cBattery.MaxChargeTemperature);
        };
        bool isDischargeTemperatureValid() const final
        {
            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_minTemperature >= cBattery.MinDischargeTemperature) && (_maxTemperature <= cBattery.MaxDischargeTemperature);
        };

        void mqttPublish() /*const*/ final;

//        void updateFrom(DalyBms::DataPointContainer const& dp);

        bool initialized() const final { return _initialized; };

    private:
        void setSoC(uint8_t value) { _SoC = value; _lastUpdateSoC = millis(); }
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        mutable uint32_t _lastMqttPublish = 0;
        mutable uint32_t _lastFullMqttPublish = 0;

        typedef struct {
            float maxCellVoltage;
            float minCellVoltage;
            float maxPackVoltage;
            float minPackVoltage;
            float maxPackChargeCurrent;
            float maxPackDischargeCurrent;
            float maxSoc;
            float minSoc;
            float cellVoltageDifference;
            float temperatureDifference;
        } AlarmWarningValues_t;
        AlarmWarningValues_t AlarmValues;
        AlarmWarningValues_t WarningValues;
        Alarm_t Alarm;
        Warning_t Warning;

        #pragma pack(push, 1)
        typedef union {
            uint8_t bytes[7];
            struct {
                unsigned levelOneCellVoltageTooHigh:1;
                unsigned levelTwoCellVoltageTooHigh:1;
                unsigned levelOneCellVoltageTooLow:1;
                unsigned levelTwoCellVoltageTooLow:1;
                unsigned levelOnePackVoltageTooHigh:1;
                unsigned levelTwoPackVoltageTooHigh:1;
                unsigned levelOnePackVoltageTooLow:1;
                unsigned levelTwoPackVoltageTooLow:1;

                unsigned levelOneChargeTempTooHigh:1;
                unsigned levelTwoChargeTempTooHigh:1;
                unsigned levelOneChargeTempTooLow:1;
                unsigned levelTwoChargeTempTooLow:1;
                unsigned levelOneDischargeTempTooHigh:1;
                unsigned levelTwoDischargeTempTooHigh:1;
                unsigned levelOneDischargeTempTooLow:1;
                unsigned levelTwoDischargeTempTooLow:1;

                unsigned levelOneChargeCurrentTooHigh:1;
                unsigned levelTwoChargeCurrentTooHigh:1;
                unsigned levelOneDischargeCurrentTooHigh:1;
                unsigned levelTwoDischargeCurrentTooHigh:1;
                unsigned levelOneStateOfChargeTooHigh:1;
                unsigned levelTwoStateOfChargeTooHigh:1;
                unsigned levelOneStateOfChargeTooLow:1;
                unsigned levelTwoStateOfChargeTooLow:1;

                unsigned levelOneCellVoltageDifferenceTooHigh:1;
                unsigned levelTwoCellVoltageDifferenceTooHigh:1;
                unsigned levelOneTempSensorDifferenceTooHigh:1;
                unsigned levelTwoTempSensorDifferenceTooHigh:1;
                unsigned dummy1:4;

                unsigned chargeFETTemperatureTooHigh:1;
                unsigned dischargeFETTemperatureTooHigh:1;
                unsigned failureOfChargeFETTemperatureSensor:1;
                unsigned failureOfDischargeFETTemperatureSensor:1;
                unsigned failureOfChargeFETAdhesion:1;
                unsigned failureOfDischargeFETAdhesion:1;
                unsigned failureOfChargeFETBreaker:1;
                unsigned failureOfDischargeFETBreaker:1;

                unsigned failureOfAFEAcquisitionModule:1;
                unsigned failureOfVoltageSensorModule:1;
                unsigned failureOfTemperatureSensorModule:1;
                unsigned failureOfEEPROMStorageModule:1;
                unsigned failureOfRealtimeClockModule:1;
                unsigned failureOfPrechargeModule:1;
                unsigned failureOfVehicleCommunicationModule:1;
                unsigned failureOfIntranetCommunicationModule:1;

                unsigned failureOfCurrentSensorModule:1;
                unsigned failureOfMainVoltageSensorModule:1;
                unsigned failureOfShortCircuitProtection:1;
                unsigned failureOfLowVoltageNoCharging:1;
                unsigned dummy2:4;
            };
        } FailureStatus_t;
        #pragma pack(pop)

        float _ratedCapacity;
        float _ratedCellVoltage;
        uint8_t _numberOfAcquisitionBoards;
        uint8_t _numberOfCellsBoard[3];
        uint8_t _numberOfNtcsBoard[3];
        uint32_t _cumulativeChargeCapacity;
        uint32_t _cumulativeDischargeCapacity;
        uint8_t _batteryType;
        char _batteryProductionDate[32];
        uint16_t _bmsSleepTime;
        float _currentWave;
        char _batteryCode[32];
        int16_t _shortCurrent;
        float _currentSamplingResistance;
        float _balanceStartVoltage;
        float _balanceDifferenceVoltage;
        char _bmsSWversion[32];
        char _bmsHWversion[32];
        float _voltage;
        float _current;
        float _batteryLevel;
        float _maxCellVoltage;
        uint8_t _maxCellVoltageNumber;
        float _minCellVoltage;
        uint8_t _minCellVoltageNumber;
        float _maxTemperature;
        uint8_t _maxTemperatureProbeNumber;
        float _minTemperature;
        uint8_t _minTemperatureProbeNumber;
        char _status[16];
        bool _chargingMosEnabled;
        bool _dischargingMosEnabled;
        uint8_t _bmsCycles;
        float _remainingCapacity;
        uint8_t _cellsNumber;
        uint8_t _tempsNumber;
        uint8_t _chargeState;
        uint8_t _loadState;
        char _dio[9];
        uint16_t  _batteryCycles;
        int _temperature[24];
        float _cellVoltage[24];
        char _cellBalance[32];
//        char _failureStatus[71];
        FailureStatus_t FailureStatus;
        uint8_t _faultCode;


        bool _initialized = false;
};
#endif

#ifdef USE_VICTRON_SMART_SHUNT
class VictronSmartShuntStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() /*const*/ final;

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
        void mqttPublish() /*const*/ final { }

        // the SoC is the only interesting value in this case, which is already
        // displayed at the top of the live view. do not generate a card.
        void getLiveViewData(JsonVariant& root) const final { }

        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = _lastUpdate = millis(); }
};
#endif
