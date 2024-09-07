// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "Arduino.h"
#include "AsyncJson.h"
#include "JkBmsDataPoints.h"
#include "JbdBmsDataPoints.h"
#include "VeDirectShuntController.h"
#include "Configuration.h"
#include "defaults.h"
#include <cfloat>

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
        unsigned overTemperatureCharge : 1;
        unsigned underTemperatureCharge : 1;
    };
    uint16_t alarms;
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
        unsigned lowTemperatureCharge : 1;
        unsigned highTemperatureCharge : 1;
    };
    uint16_t warnings;
} Warning_t;
#pragma pack(pop)

// mandatory interface for all kinds of batteries
class BatteryStats {
    public:
        String const& getManufacturer() const { return _manufacturer; }
        String const& getFwVersion() const { return _fwversion; }

        // the last time *any* data was updated
        uint32_t getAgeSeconds() const { return (millis() - _lastUpdate) / 1000; }
        bool updateAvailable(uint32_t since) const;

        float getSoC() const { return _SoC; }
        uint32_t getSoCAgeSeconds() const { return (millis() - _lastUpdateSoC) / 1000; }
        uint8_t getSoCPrecision() const { return _socPrecision; }

        // we don't need a card in the liveview, since the SoC and
        // voltage (if available) is already displayed at the top.
        virtual void getLiveViewData(JsonVariant& root) const;
        virtual void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const;

        virtual uint8_t get_number_of_packs() const { return 1; };

        void mqttLoop();

        // the interval at which all battery data will be re-published, even
        // if they did not change. used to calculate Home Assistent expiration.
        virtual uint32_t getMqttFullPublishIntervalMs() const;

        virtual Alarm_t const& getAlarm() const { static Alarm_t dummy; return dummy; };
        virtual Warning_t const& getWarning() const { static Warning_t dummy; return dummy; };
        virtual bool getChargeEnabled() const { return true; };
        virtual bool getDischargeEnabled() const { return true; };
        virtual bool getImmediateChargingRequest() const { return false; };
        virtual bool getFullChargeRequest() const { return false; };

        float getVoltage() const { return _voltage; };
        uint32_t getVoltageAgeSeconds() const { return (millis() - _lastUpdateVoltage) / 1000; }

        float getChargeCurrent() const { return _current; };
        uint8_t getChargeCurrentPrecision() const { return _currentPrecision; }

        virtual float getTemperature() const { return 0.0; };

        virtual float getRecommendedChargeVoltageLimit() const { return 0.0; };
        virtual float getRecommendedDischargeVoltageLimit() const { return 0.0; };
        virtual float getRecommendedChargeCurrentLimit() const { return 0.0; };
        virtual float getRecommendedDischargeCurrentLimit() const { return 0.0; };

        virtual float getMaximumChargeCurrentLimit() const { return 0.0; };
        virtual float getMaximumDischargeCurrentLimit() const { return 0.0; };

        virtual bool isChargeTemperatureValid() const { return false; };
        virtual bool isDischargeTemperatureValid() const { return false; };

        bool isSoCValid() const { return _lastUpdateSoC > 0; }
        bool isVoltageValid() const { return _lastUpdateVoltage > 0; }
        bool isCurrentValid() const { return _lastUpdateCurrent > 0; }

        virtual float getChargeCurrentLimitation() const { return FLT_MAX; };
        virtual float getDischargeCurrentLimitation() const { return FLT_MAX; };

    protected:
        virtual void mqttPublish() /*const*/;

        void setSoC(float soc, uint8_t precision, uint32_t timestamp) {
            _SoC = soc;
            _socPrecision = precision;
            _lastUpdateSoC = _lastUpdate = timestamp;;
        }

        void setVoltage(float voltage, uint32_t timestamp) {
            _voltage = voltage;
            _lastUpdateVoltage = _lastUpdate = timestamp;
        }

        void setCurrent(float current, uint8_t precision, uint32_t timestamp) {
            _current = current;
            _currentPrecision = precision;
            _lastUpdateCurrent = _lastUpdate = timestamp;
        }

        void setManufacturer(const String& m);

        String _hwversion = "";
        String _fwversion = "";
        String _serial = "";
        uint32_t _lastUpdate = 0;

    private:
        String _manufacturer = "unknown";
        uint32_t _lastMqttPublish = 0;
        float _SoC = 0;
        uint8_t _socPrecision = 0; // decimal places
        uint32_t _lastUpdateSoC = 0;
        float _voltage = 0; // total battery pack voltage
        uint32_t _lastUpdateVoltage = 0;

        // total current into (positive) or from (negative)
        // the battery, i.e., the charging current
        float _current = 0;
        uint8_t _currentPrecision = 0; // decimal places
        uint32_t _lastUpdateCurrent = 0;
};

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

#pragma pack(push, 1)
typedef struct {
    uint8_t commandValue; // construct.Byte,
    uint8_t numberOfCells; // Int8ub,
    uint8_t cellVoltages[15];
    uint8_t numberOfTemperatures; // Int8ub
    uint8_t BMSTemperature;
    uint8_t Temperatures[6];
    uint8_t chargeCurrent;
    uint8_t moduleVoltage;
    uint8_t dischargeCurrent;
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
    union {
        uint8_t status2;
        struct {
            unsigned preMOSFET : 1;
            unsigned chargeMOSFET : 1;
            unsigned dischargeMOSFET : 1;
            unsigned usingBatteryModulePower : 1;
            unsigned dummy4 : 4;
        };
    };
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
    union {
        struct {
            uint8_t status4;
            uint8_t status5;
        };
        uint16_t cellError;
    };
} AlarmInfo_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t commandValue; // construct.Byte,
    float chargeVoltageLimit; // construct.Array(2, construct.Byte),
    float dischargeVoltageLimit; // construct.Array(2, construct.Byte),
    float chargeCurrentLimit; // construct.Array(2, construct.Byte),
    float dischargeCurrentLimit; // construct.Array(2, construct.Byte),
    union {
        uint8_t Status; // construct.Byte,
        struct {
            unsigned dummy : 3;
            unsigned fullChargeRequest : 1; // if SOC never higher than 97% in 30 days, will set this flag to 1. And when the SOC is ≥ 97%, the flag will be 0.
            unsigned chargeImmediately2 : 1; // 9-13%
            unsigned chargeImmediately1 : 1; // 5-9%
            unsigned dischargeEnabled : 1; // 1: yes；0: request stop discharge
            unsigned chargeEnabled : 1; // 1: yes；0: request stop charge
        };
    };
} ChargeDischargeManagementInfo_t;
#pragma pack(pop)

typedef struct {
    uint8_t commandValue; // construct.Byte,
    char moduleSerialNumber[17]; // JoinBytes(construct.Array(16, construct.Byte))
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
    bool getImmediateChargingRequest() const final { return chargeImmediately; };
    bool isChargeTemperatureValid() const final { return true; }; // FIXME: to be done
    bool isDischargeTemperatureValid() const final { return true; }; // FIXME: to be done

    void mqttPublish() /*const*/ final;
    float getChargeCurrentLimitation() const { return chargeCurrentLimit; } ;
    float getDischargeCurrentLimitation() const { return dischargeCurrentLimit; } ;

    uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    uint8_t _number_of_packs = 0;

    float chargeVoltage;
    float chargeCurrentLimit;
    float dischargeCurrentLimit;
    float dischargeVoltage;

    uint16_t stateOfHealth;
//    float voltage; // total voltage of the battery pack
    // total current into (positive) or from (negative)
    // the battery, i.e., the charging current
    float temperature;
    float power;

    Alarm_t Alarm;
    Warning_t Warning;

    bool chargeEnabled;
    bool dischargeEnabled;
    bool chargeImmediately;
    bool chargeImmediately1;
    bool fullChargeRequest;

    uint8_t moduleCount;

    struct {
        float chargeVoltage;
        float chargeCurrentLimit;
        float dischargeCurrentLimit;
        float dischargeVoltageLimit;
        float dischargeVoltage;

        uint16_t stateOfHealth;
//        float voltage; // total voltage of the battery pack
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
};
#endif

#ifdef USE_PYLONTECH_RS485_RECEIVER
class PylontechRS485BatteryStats : public BatteryStats {
    friend class PylontechRS485Receiver;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;

    const Alarm_t& getAlarm() const final { return totals.Alarm; };
    const Warning_t& getWarning() const final { return totals.Warning; };
    bool getChargeEnabled() const final { return totals.ChargeDischargeManagementInfo.chargeEnabled; };
    bool getDischargeEnabled() const final { return totals.ChargeDischargeManagementInfo.dischargeEnabled; };
    bool getImmediateChargingRequest() const final {
        return (   totals.ChargeDischargeManagementInfo.chargeImmediately1
                || totals.ChargeDischargeManagementInfo.chargeImmediately2
                || totals.ChargeDischargeManagementInfo.fullChargeRequest);
    };
    bool getFullChargeRequest() const final { return totals.ChargeDischargeManagementInfo.fullChargeRequest; }
    float getTemperature() const final { return totals.averageCellTemperature; };

    float getRecommendedChargeVoltageLimit() const final { return totals.ChargeDischargeManagementInfo.chargeVoltageLimit; };
    float getRecommendedDischargeVoltageLimit() const final { return totals.ChargeDischargeManagementInfo.dischargeVoltageLimit; };
    float getRecommendedChargeCurrentLimit() const final { return totals.ChargeDischargeManagementInfo.chargeCurrentLimit; };
    float getChargeCurrentLimitation() const { return totals.ChargeDischargeManagementInfo.chargeCurrentLimit; } ;
    float getRecommendedDischargeCurrentLimit() const final { return totals.ChargeDischargeManagementInfo.dischargeCurrentLimit; };
    float getDischargeCurrentLimitation() const { return fabs(totals.ChargeDischargeManagementInfo.dischargeCurrentLimit); } ;
    float getMaximumChargeCurrentLimit() const final { return totals.SystemParameters.chargeCurrentLimit; };
    float getMaximumDischargeCurrentLimit() const final { return totals.SystemParameters.dischargeCurrentLimit; };

    bool isChargeTemperatureValid() const final
    {
        const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
        return (totals.minCellTemperature >= max(totals.SystemParameters.chargeLowTemperatureLimit, static_cast<float>(cBattery.MinChargeTemperature)))
            && (totals.maxCellTemperature <= min(totals.SystemParameters.chargeHighTemperatureLimit, static_cast<float>(cBattery.MaxChargeTemperature)));
    };
    bool isDischargeTemperatureValid() const final
    {
        const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
        return (totals.minCellTemperature >= max(totals.SystemParameters.dischargeLowTemperatureLimit, static_cast<float>(cBattery.MinDischargeTemperature)))
            && (totals.maxCellTemperature <= min(totals.SystemParameters.dischargeHighTemperatureLimit, static_cast<float>(cBattery.MaxDischargeTemperature)));
    };

    void mqttPublish() /*const*/ final;

    uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    uint8_t _number_of_packs = 0;

    struct Totals_t {
        float voltage;
        float current;
        float power;
        float capacity;
        float remainingCapacity;
        float SoC;
        uint16_t cycles;
        float cellMinVoltage;
        float cellMaxVoltage;
        float cellDiffVoltage;
        float averageBMSTemperature;
        float averageCellTemperature;
        float minCellTemperature;
        float maxCellTemperature;
        SystemParameters_t SystemParameters;
        ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
        Alarm_t Alarm;
        Warning_t Warning;
    };

    Totals_t totals;

    struct Pack_t {
        String deviceName = "";
        String softwareVersion;
        String manufacturerVersion;
        String mainLineVersion;

        float voltage;
        float current;
        float power;
        float capacity;
        float remainingCapacity;
        float SoC;

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

        SystemParameters_t SystemParameters;
        Alarm_t Alarm;
        Warning_t Warning;

        ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
        ModuleSerialNumber_t ModuleSerialNumber;
    };

    Pack_t Pack[MAX_BATTERIES];

    struct {
        float voltage;
        float current;
        float power;
        float capacity;
        float remainingCapacity;
        uint16_t cycles;

        float cellMinVoltage;
        float cellMaxVoltage;
        float cellDiffVoltage;

        float averageBMSTemperature;
        float minCellTemperature;
        float maxCellTemperature;
        float* CellVoltages;
        float* GroupedCellsTemperatures;

        ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
        SystemParameters_t SystemParameters;

        Alarm_t Alarm;
        Warning_t Warning;
    } _lastTotals;

    struct {
        String softwareVersion;
        String deviceName;

        float voltage;
        float current;
        float power;
        float capacity;
        float remainingCapacity;
        uint16_t cycles;

        float cellMinVoltage;
        float cellMaxVoltage;
        float cellDiffVoltage;
        float* CellVoltages;

        float averageBMSTemperature;
        float* GroupedCellsTemperatures;

        ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;
        SystemParameters_t SystemParameters;

        Alarm_t Alarm;
        Warning_t Warning;
    } _lastPack[MAX_BATTERIES];
};
#endif

#ifdef USE_GOBEL_RS485_RECEIVER
class GobelRS485BatteryStats : public BatteryStats {
    friend class GobelRS485Receiver;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;

    const Alarm_t& getAlarm() const final { return totals.Alarm; };
    const Warning_t& getWarning() const final { return totals.Warning; };
    bool getChargeEnabled() const final { return totals.ChargeDischargeManagementInfo.chargeEnabled; };
    bool getDischargeEnabled() const final { return totals.ChargeDischargeManagementInfo.dischargeEnabled; };
    bool getImmediateChargingRequest() const final { return false; }

private:
}
#endif

#ifdef USE_PYTES_CAN_RECEIVER
class PytesBatteryStats : public BatteryStats {
    friend class PytesCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;
        void mqttPublish() /* const */ final;
        bool getImmediateChargingRequest() const { return _chargeImmediately; } ;
        float getChargeCurrentLimitation() const { return _chargeCurrentLimit; } ;
        float getDischargeCurrentLimitation() const { return _dischargeCurrentLimit; } ;

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }
        void updateSerial() {
            if (!_serialPart1.isEmpty() && !_serialPart2.isEmpty()) {
                _serial = _serialPart1 + _serialPart2;
            }
        }

        String _serialPart1 = "";
        String _serialPart2 = "";
        String _serial = "";

        float _chargeVoltageLimit;
        float _chargeCurrentLimit;
        float _dischargeVoltageLimit;
        float _dischargeCurrentLimit;

        uint16_t _stateOfHealth;
        int _chargeCycles = -1;
        int _balance = -1;

        float _temperature;

        uint16_t _cellMinMilliVolt;
        uint16_t _cellMaxMilliVolt;
        float _cellMinTemperature;
        float _cellMaxTemperature;

        String _cellMinVoltageName;
        String _cellMaxVoltageName;
        String _cellMinTemperatureName;
        String _cellMaxTemperatureName;

        uint8_t _moduleCountOnline;
        uint8_t _moduleCountOffline;

        uint8_t _moduleCountBlockingCharge;
        uint8_t _moduleCountBlockingDischarge;

        float _totalCapacity;
        float _availableCapacity;
        uint8_t _capacityPrecision = 0; // decimal places

        float _chargedEnergy = -1;
        float _dischargedEnergy = -1;

        Alarm_t Alarm;
        Warning_t Warning;

        bool _chargeImmediately;
};
#endif

#ifdef USE_SBS_CAN_RECEIVER
class SBSBatteryStats : public BatteryStats {
    friend class SBSCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;
        void mqttPublish() /*const*/ final;
        float getChargeCurrent() const { return _current; } ;
        float getChargeCurrentLimitation() const { return _chargeCurrentLimit; } ;
        float getDischargeCurrentLimitation() const { return _dischargeCurrentLimit; } ;

        uint8_t get_number_of_packs() const final { return 1; };

        const Alarm_t& getAlarm() const final { return Alarm; };
        const Warning_t& getWarning() const final { return Warning; };

        bool getChargeEnabled() const final { return _chargeEnabled; };
        bool getDischargeEnabled() const final { return _dischargeEnabled; };

        bool isChargeTemperatureValid() const final
        {
            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_temperature >= cBattery.MinChargeTemperature) && (_temperature <= cBattery.MaxChargeTemperature);
        };
        bool isDischargeTemperatureValid() const final
        {
            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_temperature >= cBattery.MinDischargeTemperature) && (_temperature <= cBattery.MaxDischargeTemperature);
        };

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimit;
        float _dischargeCurrentLimit;
        uint16_t _stateOfHealth;
        float _current;
        float _temperature;

        Alarm_t Alarm;
        Warning_t Warning;

        bool _chargeEnabled;
        bool _dischargeEnabled;
};
#endif

#ifdef USE_JKBMS_CONTROLLER
class JkBmsBatteryStats : public BatteryStats {
    friend class Controller;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;

    uint8_t get_number_of_packs() const final { return 1; };

    const Alarm_t& getAlarm() const final { return Alarm; };
    const Warning_t& getWarning() const final { return Warning; };

    bool getChargeEnabled() const final { return ChargeDischargeManagementInfo.chargeEnabled; };
    bool getDischargeEnabled() const final { return ChargeDischargeManagementInfo.dischargeEnabled; };
    bool getImmediateChargingRequest() const final { return ChargeDischargeManagementInfo.chargeImmediately1; };

    float getRecommendedChargeVoltageLimit() const final { return ChargeDischargeManagementInfo.chargeVoltageLimit; };
    float getRecommendedDischargeVoltageLimit() const final { return ChargeDischargeManagementInfo.dischargeVoltageLimit; };
    float getRecommendedChargeCurrentLimit() const final { return ChargeDischargeManagementInfo.chargeCurrentLimit; };
    float getChargeCurrentLimitation() const { return ChargeDischargeManagementInfo.chargeCurrentLimit; } ;
    float getDischargeCurrentLimitation() const { return ChargeDischargeManagementInfo.dischargeCurrentLimit; } ;

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

    void updateFrom(JkBms::DataPointContainer const& dp);

private:
    JkBms::DataPointContainer _dataPoints;

    float current;
    int16_t _minTemperature = 100;
    int16_t _maxTemperature = -100;
    Alarm_t Alarm;
    Warning_t Warning;
    ChargeDischargeManagementInfo_t ChargeDischargeManagementInfo;

    mutable uint32_t _lastMqttPublish = 0;
    mutable uint32_t _lastFullMqttPublish = 0;

    uint16_t _cellMinMilliVolt = 0;
    uint16_t _cellAvgMilliVolt = 0;
    uint16_t _cellMaxMilliVolt = 0;
    uint32_t _cellVoltageTimestamp = 0;
};

#endif

#ifdef USE_DALYBMS_CONTROLLER
class DalyBmsBatteryStats : public BatteryStats {
    friend class DalyBmsController;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const final;

        const Alarm_t& getAlarm() const final { return Alarm; };
        const Warning_t& getWarning() const final { return Warning; };
        bool getChargeEnabled() const final { return chargingMosEnabled; };
        bool getDischargeEnabled() const final { return dischargingMosEnabled; };
        bool getImmediateChargingRequest() const final { return chargeImmediately1 || chargeImmediately2; };
        float getTemperature() const final { return (_maxTemperature + _minTemperature) / 2.0; };

        float getRecommendedChargeVoltageLimit() const final { return WarningValues.maxPackVoltage * 0.99; };  // 1% below warning level
        float getRecommendedDischargeVoltageLimit() const final { return WarningValues.minPackVoltage * 1.01; }; // 1% above warning level
        float getRecommendedChargeCurrentLimit() const final { return WarningValues.maxPackChargeCurrent * 0.9; }; // 10% below warning level
        float getChargeCurrentLimitation() const final { return WarningValues.maxPackChargeCurrent * 0.9; };
        float getRecommendedDischargeCurrentLimit() const final { return WarningValues.maxPackDischargeCurrent * 0.9; }; // 10% below warning level
        float getDischargeCurrentLimitation() const final { return WarningValues.maxPackDischargeCurrent * 0.9; }; // 10% below warning level
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

    private:
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

        struct {
            // float voltage;
            float current;
            float power;
            float ratedCapacity;
            float remainingCapacity;
            uint16_t  batteryCycles;
            Warning_t Warning;
            Alarm_t Alarm;
            bool chargingMosEnabled;
            bool dischargingMosEnabled;
            bool chargeImmediately1;
            bool chargeImmediately2;
            bool cellBalanceActive;
            float maxCellVoltage;
            float minCellVoltage;
            float cellDiffVoltage;
            float *_cellVoltage;
            int *_temperature;
            int averageBMSTemperature;
        } _last;

        float ratedCapacity;
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
        float currentSamplingResistance;
        float _balanceStartVoltage;
        float _balanceDifferenceVoltage;
        char _bmsSWversion[32];
        char _bmsHWversion[32];
        // float voltage;
        float gatherVoltage;
        float current;
        float power;
        float batteryLevel;
        float maxCellVoltage;
        uint8_t _maxCellVoltageNumber;
        float minCellVoltage;
        uint8_t _minCellVoltageNumber;
        float cellDiffVoltage;
        float _maxTemperature;
        uint8_t _maxTemperatureProbeNumber;
        float _minTemperature;
        uint8_t _minTemperatureProbeNumber;
        char _status[16];
        bool chargingMosEnabled;
        bool dischargingMosEnabled;
        bool chargeImmediately1;
        bool chargeImmediately2;
        bool cellBalanceActive;
        uint8_t _bmsCycles;
        float remainingCapacity;
        uint8_t _cellsNumber;
        uint8_t _tempsNumber;
        uint8_t _chargeState;
        uint8_t _loadState;
        uint8_t _dIO;
        uint16_t  batteryCycles;
        int *_temperature;
        int averageBMSTemperature;
        float *_cellVoltage;
        char _cellBalance[6];
        FailureStatus_t FailureStatus;
        uint8_t _faultCode;
};
#endif

#ifdef USE_JBDBMS_CONTROLLER
class JbdBmsBatteryStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final {
            getJsonData(root, false);
        }

        void getInfoViewData(JsonVariant& root) const {
            getJsonData(root, true);
        }

        void mqttPublish() const final;

        uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

        void updateFrom(JbdBms::DataPointContainer const& dp);

    private:
        void getJsonData(JsonVariant& root, bool verbose) const;

        JbdBms::DataPointContainer _dataPoints;
        mutable uint32_t _lastMqttPublish = 0;
        mutable uint32_t _lastFullMqttPublish = 0;

        uint16_t _cellMinMilliVolt = 0;
        uint16_t _cellAvgMilliVolt = 0;
        uint16_t _cellMaxMilliVolt = 0;
        uint32_t _cellVoltageTimestamp = 0;
};
#endif

#ifdef USE_VICTRON_SMART_SHUNT
class VictronSmartShuntStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() /*const*/ final;

        void updateFrom(VeDirectShuntController::data_t const& shuntData);

        float getRecommendedChargeVoltageLimit() const final {
            // FIXME
            return Configuration.get().Battery.RecommendedChargeVoltage;
        };
        float getRecommendedDischargeVoltageLimit() const final {
            // FIXME
            return Configuration.get().Battery.RecommendedDischargeVoltage;
        };
        float getRecommendedChargeCurrentLimit() const final { return 50.0; }; // FIXME
        float getRecommendedDischargeCurrentLimit() const final { return 50.0; }; // FIXME

        bool getImmediateChargingRequest() const final { return ( getSoC() < 5.0); };  // FIXME below 5%
        bool getFullChargeRequest() const final { return _lastFullCharge > 24*60*45; }; // FIXME 45 days

        bool isChargeTemperatureValid() const final
        {
            if (!_tempPresent) return true; // if temperature sensor not present always return true

            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_temperature >= static_cast<float>(cBattery.MinChargeTemperature))
                && (_temperature <= static_cast<float>(cBattery.MaxChargeTemperature));
        };
        bool isDischargeTemperatureValid() const final
        {
            if (!_tempPresent) return true; // if temperature sensor not present always return true

            const Battery_CONFIG_T& cBattery = Configuration.get().Battery;
            return (_temperature >= static_cast<float>(cBattery.MinDischargeTemperature))
                && (_temperature <= static_cast<float>(cBattery.MaxDischargeTemperature));
        };

    private:
        float _temperature;
        bool _tempPresent;
        uint8_t _chargeCycles;
        uint32_t _timeToGo;
        float _chargedEnergy;
        float _dischargedEnergy;
        int32_t _instantaneousPower;
        float _midpointVoltage;
        float _midpointDeviation;
        float _consumedAmpHours;
        int32_t _lastFullCharge;

        bool _alarmLowVoltage;
        bool _alarmHighVoltage;
        bool _alarmLowSOC;
        bool _alarmLowTemperature;
        bool _alarmHighTemperature;
};
#endif

#ifdef USE_MQTT_BATTERY
class MqttBatteryStats : public BatteryStats {
    friend class MqttBattery;

    public:
        // since the source of information was MQTT in the first place,
        // we do NOT publish the same data under a different topic.
        void mqttPublish() /*const*/ final { }

        // if the voltage is subscribed to at all, it alone does not warrant a
        // card in the live view, since the SoC is already displayed at the top
        void getLiveViewData(JsonVariant& root) const final { }
};
#endif

#ifdef USE_MQTT_ZENDURE_BATTERY
class ZendureBatteryStats : public BatteryStats {
    friend class ZendureBattery;

    class ZendurePackStats {
        friend class ZendureBatteryStats;

        public:
            explicit ZendurePackStats(String serial){ setSerial(serial); }
            void update(JsonObjectConst packData, unsigned int ms);
            bool isCharging() const { return  _state == 2; };
            bool isDischarging() const { return  _state == 1; };
            uint16_t getCapacity() const { return 1920; }
            std::string getStateString() const { return ZendureBatteryStats::getStateString(_state); };

        protected:
            bool hasAlarmMaxTemp() const { return _cell_temperature_max >= 45; };
            bool hasAlarmMinTemp() const { return _cell_temperature_max <= (isCharging() ? 0 : -20); };
            bool hasAlarmLowSoC() const { return _soc_level < 5; }
            bool hasAlarmLowVoltage() const { return _voltage_total <= 40.0; }
            bool hasAlarmHighVoltage() const { return _voltage_total >= 58.4; }
            void setSerial(String serial);

            String _serial;
            String _name = "unknown";
            uint16_t _capacity = 0;
            uint32_t _version;
            uint16_t _cell_voltage_min;
            uint16_t _cell_voltage_max;
            uint16_t _cell_voltage_spread;
            float _cell_temperature_max;
            float _voltage_total;
            float _current;
            int16_t _power;
            uint8_t _soc_level;
            uint8_t _state;

        private:
            uint32_t _lastUpdateTimestamp = 0;
            uint32_t _totalVoltageTimestamp = 0;
            uint32_t _totalCurrentTimestamp = 0;

    };

    public:
        virtual ~ZendureBatteryStats(){
            for (const auto& [key, item] : _packData){
                delete item;
            }
            _packData.clear();
        }
        void mqttPublish() const;
        void getLiveViewData(JsonVariant& root) const;

        bool isCharging() const { return  _state == 1; };
        bool isDischarging() const { return  _state == 2; };

        static std::string getStateString(uint8_t state);

    protected:
        std::optional<ZendureBatteryStats::ZendurePackStats*> getPackData(String serial) const;
        void updatePackData(String serial, JsonObjectConst packData, unsigned int ms);
        void update(JsonObjectConst props, unsigned int ms);
        uint16_t getCapacity() const { return _capacity; };
        uint16_t getAvailableCapacity() const { return getCapacity() * (static_cast<float>(_soc_max - _soc_min) / 100.0); };

    private:
        std::string getBypassModeString() const;
        std::string getStateString() const { return ZendureBatteryStats::getStateString(_state); };
        void calculateEfficiency();
        void calculateAggregatedPackData();

        void setManufacture(const char* manufacture) {
            setManufacturer(manufacture);
            if (_device){
                setManufacturer(String(manufacture) + " " + _device);
            }
        }

        void setSerial(const char* serial) {
            _serial = String(serial);
        }
        void setSerial(String serial) {
            _serial = serial;
        }

        void setDevice(const char* device) {
            _device = String(device);
        }
        void setDevice(String device) {
            _device = device;
        }

        String _device;

        std::map<String, ZendurePackStats*> _packData = std::map<String, ZendurePackStats*>();

        float _cellTemperature;
        uint16_t _cellMinMilliVolt;
        uint16_t _cellMaxMilliVolt;
        uint16_t _cellDeltaMilliVolt;

        float _soc_max;
        float _soc_min;

        uint16_t _inverse_max;
        uint16_t _input_limit;
        uint16_t _output_limit;

        float _efficiency = 0.0;
        uint16_t _capacity;
        uint16_t _charge_power;
        uint16_t _discharge_power;
        uint16_t _output_power;
        uint16_t _input_power;
        uint16_t _solar_power_1;
        uint16_t _solar_power_2;

        uint16_t _remain_out_time;
        uint16_t _remain_in_time;

        uint32_t _swVersion;
        uint32_t _hwVersion;

        uint8_t _state;
        uint8_t _num_batteries;
        uint8_t _bypass_mode;
        bool _bypass_state;
        bool _auto_recover;
        bool _heat_state;
        bool _auto_shutdown;
        bool _buzzer;

        bool _alarmLowSoC = false;
        bool _alarmLowVoltage = false;
        bool _alarmHightVoltage = false;
        bool _alarmLowTemperature = false;
        bool _alarmHighTemperature = false;

};
#endif
