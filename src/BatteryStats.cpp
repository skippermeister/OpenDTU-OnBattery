// SPDX-License-Identifier: GPL-2.0-or-later
#include "BatteryStats.h"
#include "Configuration.h"
#include "JkBmsDataPoints.h"
#include "MqttSettings.h"

template <typename T>
static void addLiveViewValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["values"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
static void addLiveViewParameter(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["parameters"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

static void addLiveViewText(JsonVariant& root, std::string const& name,
    std::string const& text)
{
    root["values"][name] = text;
}

static void addLiveViewWarning(JsonVariant& root, std::string const& name,
    bool warning)
{
    if (!warning) { return; }
    root["issues"][name] = 1;
}

static void addLiveViewAlarm(JsonVariant& root, std::string const& name,
    bool alarm)
{
    if (!alarm) { return; }
    root["issues"][name] = 2;
}

static void addLiveViewFailureStatus(JsonVariant& root, std::string const& name,
    bool alarm)
{
    if (!alarm) { return; }
    root["issues"][name] = 2;
}

static void addLiveViewCellVoltage(JsonVariant& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["cell"]["voltage"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "V";
    jsonValue["d"] = precision;
}

static void addLiveViewCellBalance(JsonVariant& root, std::string const& name,
    float value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["cell"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

static void addLiveViewTempSensor(JsonVariant& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["tempSensor"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "°C";
    jsonValue["d"] = precision;
}

void BatteryStats::getLiveViewData(JsonVariant& root) const
{
    root["manufacturer"] = _manufacturer;
    root["device_name"] = _deviceName;
    root["data_age"] = getAgeSeconds();

    addLiveViewValue(root, "SoC", _SoC, "%", 0);
}

#define ISSUE(token, value) addLiveView##token(root, #value, token.value > 0);

#ifdef USE_PYLONTECH_CAN_RECEIVER
void PylontechCanBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", chargeVoltage, "V", 1);
    addLiveViewValue(root, "chargeCurrentLimit", chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", dischargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "stateOfHealth", stateOfHealth, "%", 0);
    addLiveViewValue(root, "voltage", voltage, "V", 2);
    addLiveViewValue(root, "current", current, "A", 1);
    addLiveViewValue(root, "temperature", temperature, "°C", 1);

    addLiveViewText(root, "chargeEnabled", (chargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (dischargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately", (chargeImmediately ? "yes" : "no"));

    // alarms and warnings go into the "Issues" card of the web application
    ISSUE(Warning, highCurrentDischarge);
    ISSUE(Alarm, overCurrentDischarge);

    ISSUE(Warning, highCurrentCharge);
    ISSUE(Alarm, overCurrentCharge);

    ISSUE(Warning, lowTemperature);
    ISSUE(Alarm, underTemperature);

    ISSUE(Warning, highTemperature);
    ISSUE(Alarm, overTemperature);

    ISSUE(Warning, lowVoltage);
    ISSUE(Alarm, underVoltage);

    ISSUE(Warning, highVoltage);
    ISSUE(Alarm, overVoltage);

    ISSUE(Warning, bmsInternal);
    ISSUE(Alarm, bmsInternal);
}
#endif

#ifdef USE_PYLONTECH_RS485_RECEIVER
void PylontechRS485BatteryStats::getLiveViewData(JsonVariant& root) const
{
    root["software_version"] = softwareVersion;
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "totalCapacity", totalCapacity, "Ah", 3);
    addLiveViewValue(root, "remainingCapacity", remainingCapacity, "Ah", 3);
    // angeforderte Ladespannung
    addLiveViewValue(root, "chargeVoltage", chargeVoltage, "V", 3);

    addLiveViewValue(root, "cycles", cycles, "", 0);
    addLiveViewValue(root, "voltage", voltage, "V", 2);
    addLiveViewValue(root, "current", current, "A", 1);
    addLiveViewValue(root, "power", power, "kW", 3);
    addLiveViewValue(root, "BMSTemperature", averageBMSTemperature, "°C", 1);
    addLiveViewValue(root, "CellTemperature", averageCellTemperature, "°C", 1);

    // Empfohlene Lade-/Enladespannung
    addLiveViewValue(root, "chargeVoltageLimit", ChargeDischargeManagementInfo.chargeVoltageLimit, "V", 3);
    addLiveViewValue(root, "dischargeVoltageLimit", ChargeDischargeManagementInfo.dischargeVoltageLimit, "V", 3);
    // Empfohlene Lade-/Enladesstrom
    addLiveViewValue(root, "chargeCurrentLimit", ChargeDischargeManagementInfo.chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", ChargeDischargeManagementInfo.dischargeCurrentLimit, "A", 1);

    // maximale Lade-/Enladestrom (90A@15sec)
    addLiveViewValue(root, "maxChargeCurrentLimit", SystemParameters.chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "maxDischargeCurrentLimit", SystemParameters.dischargeCurrentLimit, "A", 1);

    addLiveViewParameter(root, "cellHighVoltageLimit", SystemParameters.cellHighVoltageLimit, "V", 3);
    addLiveViewParameter(root, "cellLowVoltageLimit", SystemParameters.cellLowVoltageLimit, "V", 3);
    addLiveViewParameter(root, "cellUnderVoltageLimit", SystemParameters.cellUnderVoltageLimit, "V", 3);
    addLiveViewParameter(root, "chargeHighTemperatureLimit", SystemParameters.chargeHighTemperatureLimit, "°C", 1);
    addLiveViewParameter(root, "chargeLowTemperatureLimit", SystemParameters.chargeLowTemperatureLimit, "°C", 1);
    addLiveViewParameter(root, "dischargeHighTemperatureLimit", SystemParameters.dischargeHighTemperatureLimit, "°C", 1);
    addLiveViewParameter(root, "dischargeLowTemperatureLimit", SystemParameters.dischargeLowTemperatureLimit, "°C", 1);

    addLiveViewText(root, "chargeEnabled", (ChargeDischargeManagementInfo.chargeEnable ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (ChargeDischargeManagementInfo.dischargeEnable ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately", (ChargeDischargeManagementInfo.chargeImmediately ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately1", (ChargeDischargeManagementInfo.chargeImmediately1 ? "yes" : "no"));
    addLiveViewText(root, "fullChargeRequest", (ChargeDischargeManagementInfo.fullChargeRequest ? "yes" : "no"));

    // alarms and warnings go into the "Issues" card of the web application
    ISSUE(Warning, highCurrentDischarge);
    ISSUE(Alarm, overCurrentDischarge);

    ISSUE(Warning, highCurrentCharge);
    ISSUE(Alarm, overCurrentCharge);

    ISSUE(Warning, lowTemperature);
    ISSUE(Alarm, underTemperature);

    ISSUE(Warning, highTemperature);
    ISSUE(Alarm, overTemperature);

    ISSUE(Warning, lowVoltage);
    ISSUE(Alarm, underVoltage);

    ISSUE(Warning, highVoltage);
    ISSUE(Alarm, overVoltage);

    addLiveViewCellBalance(root, "cellMinVoltage", cellMinVoltage, "V", 3);
    addLiveViewCellBalance(root, "cellMaxVoltage", cellMaxVoltage, "V", 3);
    addLiveViewCellBalance(root, "cellDiffVoltage", cellDiffVoltage, "mV", 0);

    // if (numberOfCells > 15) numberOfCells=15;
    for (int i = 0; i < numberOfCells; i++) {
        addLiveViewCellVoltage(root, i, CellVoltages[i], 3);
    }
    // if (numberOfTemperatures>5) numberOfTemperatures=5;
    for (int i = 0; i < numberOfTemperatures - 1; i++) {
        addLiveViewTempSensor(root, i, GroupedCellsTemperatures[i], 1);
    }
}
#endif

#ifdef USE_JKBMS_CONTROLLER
void JkBmsBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    using Label = JkBms::DataPointLabel;

    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        addLiveViewValue(root, "voltage", static_cast<float>(*oVoltage) / 1000, "V", 2);
    }

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        addLiveViewValue(root, "current", static_cast<float>(*oCurrent) / 1000, "A", 2);
    }

    if (oVoltage.has_value() && oCurrent.has_value()) {
        auto current = static_cast<float>(*oCurrent) / 1000;
        auto voltage = static_cast<float>(*oVoltage) / 1000;
        addLiveViewValue(root, "power", current * voltage, "W", 2);
    }

    auto oTemperatureBms = _dataPoints.get<Label::BmsTempCelsius>();
    if (oTemperatureBms.has_value()) {
        addLiveViewValue(root, "bmsTemp", *oTemperatureBms, "°C", 0);
    }

    // labels BatteryChargeEnabled, BatteryDischargeEnabled, and
    // BalancingEnabled refer to the user setting. we want to show the
    // actual MOSFETs' state which control whether charging and discharging
    // is possible and whether the BMS is currently balancing cells.
    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        auto chargeEnabled = *oStatus & static_cast<uint16_t>(Bits::ChargingActive);
        addLiveViewTextValue(root, "chargeEnabled", (chargeEnabled ? "yes" : "no"));
        auto dischargeEnabled = *oStatus & static_cast<uint16_t>(Bits::DischargingActive);
        addLiveViewTextValue(root, "dischargeEnabled", (dischargeEnabled ? "yes" : "no"));
    }

    auto oTemperatureOne = _dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperatureOne.has_value()) {
        addLiveViewInSection(root, "cells", "batOneTemp", *oTemperatureOne, "°C", 0);
    }

    auto oTemperatureTwo = _dataPoints.get<Label::BatteryTempTwoCelsius>();
    if (oTemperatureTwo.has_value()) {
        addLiveViewInSection(root, "cells", "batTwoTemp", *oTemperatureTwo, "°C", 0);
    }

    if (_cellVoltageTimestamp > 0) {
        addLiveViewInSection(root, "cells", "cellMinVoltage", static_cast<float>(_cellMinMilliVolt) / 1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellAvgVoltage", static_cast<float>(_cellAvgMilliVolt) / 1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt) / 1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);
    }

    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        auto balancingActive = *oStatus & static_cast<uint16_t>(Bits::BalancingActive);
        addLiveViewTextInSection(root, "cells", "balancingActive", (balancingActive ? "yes" : "no"));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
#define ISSUE(t, x)                                                 \
    auto x = *oAlarms & static_cast<uint16_t>(JkBms::AlarmBits::x); \
    addLiveView##t(root, "JkBmsIssue" #x, x > 0);

        ISSUE(Warning, LowCapacity);
        ISSUE(Alarm, BmsOvertemperature);
        ISSUE(Alarm, ChargingOvervoltage);
        ISSUE(Alarm, DischargeUndervoltage);
        ISSUE(Alarm, BatteryOvertemperature);
        ISSUE(Alarm, ChargingOvercurrent);
        ISSUE(Alarm, DischargeOvercurrent);
        ISSUE(Alarm, CellVoltageDifference);
        ISSUE(Alarm, BatteryBoxOvertemperature);
        ISSUE(Alarm, BatteryUndertemperature);
        ISSUE(Alarm, CellOvervoltage);
        ISSUE(Alarm, CellUndervoltage);
        ISSUE(Alarm, AProtect);
        ISSUE(Alarm, BProtect);
#undef ISSUE
    }
}
#endif

#ifdef USE_DALYBMS_CONTROLLER
void DalyBmsBatteryStats::getLiveViewData(JsonVariant& root) const
{
    root["software_version"] = String(_bmsSWversion);
    BatteryStats::getLiveViewData(root);

    addLiveViewValue(root, "BatteryLevel", batteryLevel, "%", 1);

    addLiveViewValue(root, "totalCapacity", ratedCapacity, "Ah", 3);
    addLiveViewValue(root, "remainingCapacity", remainingCapacity, "Ah", 3);

    addLiveViewValue(root, "cycles", batteryCycles, "", 0);
    addLiveViewValue(root, "voltage", voltage, "V", 1);
    addLiveViewValue(root, "current", current, "A", 1);
    addLiveViewValue(root, "power", power, "kW", 3);

    addLiveViewParameter(root, "cellHighVoltageLimit", AlarmValues.maxCellVoltage, "V", 3);
    addLiveViewParameter(root, "cellLowVoltageLimit", WarningValues.minCellVoltage, "V", 3);
    addLiveViewParameter(root, "cellUnderVoltageLimit", AlarmValues.minCellVoltage, "V", 3);

    addLiveViewText(root, "chargeEnabled", (chargingMosEnabled ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (dischargingMosEnabled ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately", (getChargeImmediately() ? "yes" : "no"));

    addLiveViewCellBalance(root, "cellMinVoltage", minCellVoltage, "V", 3);
    addLiveViewCellBalance(root, "cellMaxVoltage", maxCellVoltage, "V", 3);
    addLiveViewCellBalance(root, "cellDiffVoltage", cellDiffVoltage, "mV", 0);

    // Empfohlene Lade-/Enladespannung
    addLiveViewValue(root, "chargeVoltageLimit", WarningValues.maxPackVoltage, "V", 3);
    addLiveViewValue(root, "dischargeVoltageLimit", WarningValues.minPackVoltage, "V", 3);
    // Empfohlene Lade-/Enladesstrom
    addLiveViewValue(root, "chargeCurrentLimit", WarningValues.maxPackChargeCurrent, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", WarningValues.maxPackDischargeCurrent, "A", 1);

    // maximale Lade-/Enladestrom (90A@15sec)
    addLiveViewValue(root, "maxChargeCurrentLimit", AlarmValues.maxPackChargeCurrent, "A", 1);
    addLiveViewValue(root, "maxDischargeCurrentLimit", AlarmValues.maxPackDischargeCurrent, "A", 1);

    for (int i = 0; i < min(_cellsNumber, static_cast<uint8_t>(18)); i++) {
        addLiveViewCellVoltage(root, i, _cellVoltage[i], 3);
    }
    for (int i = 0; i < min(_tempsNumber, static_cast<uint8_t>(5)); i++) {
        addLiveViewTempSensor(root, i, _temperature[i], 1);
    }

    ISSUE(Warning, highCurrentDischarge);
    ISSUE(Alarm, overCurrentDischarge);

    ISSUE(Warning, highCurrentCharge);
    ISSUE(Alarm, overCurrentCharge);

    ISSUE(Warning, lowTemperature);
    ISSUE(Alarm, underTemperature);

    ISSUE(Warning, highTemperature);
    ISSUE(Alarm, overTemperature);

    ISSUE(Warning, lowVoltage);
    ISSUE(Alarm, underVoltage);

    ISSUE(Warning, highVoltage);
    ISSUE(Alarm, overVoltage);

    ISSUE(Alarm, bmsInternal);
    ISSUE(Alarm, cellImbalance);
    ISSUE(Warning, cellImbalance);

/*
    ISSUE(FailureStatus, levelOneCellVoltageTooHigh);
    ISSUE(FailureStatus, levelTwoCellVoltageTooHigh);
    ISSUE(FailureStatus, levelOneCellVoltageTooLow);
    ISSUE(FailureStatus, levelTwoCellVoltageTooLow);
    ISSUE(FailureStatus, levelOnePackVoltageTooHigh);
    ISSUE(FailureStatus, levelTwoPackVoltageTooHigh);
    ISSUE(FailureStatus, levelOnePackVoltageTooLow);
    ISSUE(FailureStatus, levelTwoPackVoltageTooLow);

    ISSUE(FailureStatus, levelOneChargeTempTooHigh);
    ISSUE(FailureStatus, levelTwoChargeTempTooHigh);
    ISSUE(FailureStatus, levelOneChargeTempTooLow);
    ISSUE(FailureStatus, levelTwoChargeTempTooLow);
    ISSUE(FailureStatus, levelOneDischargeTempTooHigh);
    ISSUE(FailureStatus, levelTwoDischargeTempTooHigh);
    ISSUE(FailureStatus, levelOneDischargeTempTooLow);
    ISSUE(FailureStatus, levelTwoDischargeTempTooLow);

    ISSUE(FailureStatus, levelOneChargeCurrentTooHigh);
    ISSUE(FailureStatus, levelTwoChargeCurrentTooHigh);
    ISSUE(FailureStatus, levelOneDischargeCurrentTooHigh);
    ISSUE(FailureStatus, levelTwoDischargeCurrentTooHigh);
    ISSUE(FailureStatus, levelOneStateOfChargeTooHigh);
    ISSUE(FailureStatus, levelTwoStateOfChargeTooHigh);
    ISSUE(FailureStatus, levelOneStateOfChargeTooLow);
    ISSUE(FailureStatus, levelTwoStateOfChargeTooLow);

    ISSUE(FailureStatus, levelOneCellVoltageDifferenceTooHigh);
    ISSUE(FailureStatus, levelTwoCellVoltageDifferenceTooHigh);
    ISSUE(FailureStatus, levelOneTempSensorDifferenceTooHigh);
    ISSUE(FailureStatus, levelTwoTempSensorDifferenceTooHigh);

    ISSUE(FailureStatus, chargeFETTemperatureTooHigh);
    ISSUE(FailureStatus, dischargeFETTemperatureTooHigh);
*/
    ISSUE(FailureStatus, failureOfChargeFETTemperatureSensor);
    ISSUE(FailureStatus, failureOfDischargeFETTemperatureSensor);
    ISSUE(FailureStatus, failureOfChargeFETAdhesion);
    ISSUE(FailureStatus, failureOfDischargeFETAdhesion);
    ISSUE(FailureStatus, failureOfChargeFETBreaker);
    ISSUE(FailureStatus, failureOfDischargeFETBreaker);

    ISSUE(FailureStatus, failureOfAFEAcquisitionModule);
    ISSUE(FailureStatus, failureOfVoltageSensorModule);
    ISSUE(FailureStatus, failureOfTemperatureSensorModule);
    ISSUE(FailureStatus, failureOfEEPROMStorageModule);
    ISSUE(FailureStatus, failureOfRealtimeClockModule);
    ISSUE(FailureStatus, failureOfPrechargeModule);
    ISSUE(FailureStatus, failureOfVehicleCommunicationModule);
    ISSUE(FailureStatus, failureOfIntranetCommunicationModule);

    ISSUE(FailureStatus, failureOfCurrentSensorModule);
    ISSUE(FailureStatus, failureOfMainVoltageSensorModule);
    ISSUE(FailureStatus, failureOfShortCircuitProtection);
    ISSUE(FailureStatus, failureOfLowVoltageNoCharging);
}
#endif

void BatteryStats::mqttLoop()
{
    auto& config = Configuration.get();

    if (!MqttSettings.getConnected()
            || (millis() - _lastMqttPublish) < (config.Mqtt.PublishInterval * 1000)) {
        return;
    }

    mqttPublish();

    _lastMqttPublish = millis();
}

uint32_t BatteryStats::getMqttFullPublishIntervalMs() const
{
    auto& config = Configuration.get();

    // this is the default interval, see mqttLoop(). mqttPublish()
    // implementations in derived classes may choose to publish some values
    // with a lower frequency and hence implement this method with a different
    // return value.
    return config.Mqtt.PublishInterval * 1000;
}

void BatteryStats::mqttPublish()  /* const */
{
    String subtopic = "battery/";
    MqttSettings.publish(subtopic + "manufacturer", _manufacturer);
    MqttSettings.publish(subtopic + "dataAge", String(getAgeSeconds()));
    MqttSettings.publish(subtopic + "stateOfCharge", String(_SoC));
}

#define MQTTpublish(value, digits)                     \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = value, digits));
#define MQTTpublishStruct(str, value, digits)                  \
    if (!cBattery.UpdatesOnly || str.value != _last.str.value) \
        MqttSettings.publish(subtopic + #value, String(_last.str.value = str.value, digits));
#define MQTTpublishInt(value)                          \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = value));
#define MQTTpublishIntStruct(str, value)                       \
    if (!cBattery.UpdatesOnly || str.value != _last.str.value) \
        MqttSettings.publish(subtopic + #value, String(_last.str.value = str.value));
#define MQTTpublishHex(value, str)                               \
    if (!cBattery.UpdatesOnly || str##value != _last.str##value) \
        MqttSettings.publish(subtopic + #value, String(_last.str##value = str##value, HEX));
#define MQTTpublishString(value)                       \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, _last.value = value);

#ifdef USE_PYLONTECH_CAN_RECEIVER
void PylontechCanBatteryStats::mqttPublish() const
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    String subtopic = "battery/settings/";
    MQTTpublish(chargeVoltage, 2);
    MQTTpublish(chargeCurrentLimit, 2);
    MQTTpublish(dischargeCurrentLimit, 2);
    MQTTpublish(dischargeVoltage, 2);

    subtopic = "battery/";
    MQTTpublishInt(stateOfHealth);
    MQTTpublish(voltage, 2);
    MQTTpublish(current, 2);
    MQTTpublish(temperature, 1);
    MQTTpublish(power, 3);

    subtopic = "battery/alarms/";
    MQTTpublishIntStruct(Alarm, overCurrentDischarge);
    MQTTpublishIntStruct(Alarm, overCurrentCharge);
    MQTTpublishIntStruct(Alarm, underTemperature);
    MQTTpublishIntStruct(Alarm, overTemperature);
    MQTTpublishIntStruct(Alarm, underVoltage);
    MQTTpublishIntStruct(Alarm, overVoltage);
    MQTTpublishIntStruct(Alarm, bmsInternal);

    subtopic = "battery/warnings/";
    MQTTpublishIntStruct(Warning, highCurrentDischarge);
    MQTTpublishIntStruct(Warning, highCurrentCharge);
    MQTTpublishIntStruct(Warning, lowTemperature);
    MQTTpublishIntStruct(Warning, highTemperature);
    MQTTpublishIntStruct(Warning, lowVoltage);
    MQTTpublishIntStruct(Warning, highVoltage);
    MQTTpublishIntStruct(Warning, bmsInternal);

    subtopic = "battery/charging/";
    MQTTpublishInt(chargeEnabled);
    MQTTpublishInt(dischargeEnabled);
    MQTTpublishInt(chargeImmediately);
}
#endif

#ifdef USE_PYLONTECH_RS485_RECEIVER
void PylontechRS485BatteryStats::mqttPublish() /*const*/
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    String subtopic = "battery/settings/";
    MQTTpublish(chargeVoltage, 2);
    MQTTpublishStruct(ChargeDischargeManagementInfo, chargeCurrentLimit, 2)
    MQTTpublishStruct(ChargeDischargeManagementInfo, dischargeCurrentLimit, 2);

    subtopic = "battery/";
    MQTTpublishString(softwareVersion);
    MQTTpublishInt(cycles);
    MQTTpublish(voltage, 2);
    MQTTpublish(current, 2);
    MQTTpublish(power, 3);
    MQTTpublish(remainingCapacity, 3);
    MQTTpublish(totalCapacity, 3);

    subtopic = "battery/parameters/";
    MQTTpublishStruct(SystemParameters, chargeCurrentLimit, 2)
        MQTTpublishStruct(SystemParameters, dischargeCurrentLimit, 2);
    MQTTpublishStruct(SystemParameters, cellHighVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, cellLowVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, cellUnderVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, chargeHighTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, chargeLowTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, dischargeHighTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, dischargeLowTemperatureLimit, 1);

    subtopic = "battery/alarms/";
    MQTTpublishIntStruct(Alarm, overCurrentDischarge);
    MQTTpublishIntStruct(Alarm, overCurrentCharge);
    MQTTpublishIntStruct(Alarm, underTemperature);
    MQTTpublishIntStruct(Alarm, overTemperature);
    MQTTpublishIntStruct(Alarm, underVoltage);
    MQTTpublishIntStruct(Alarm, overVoltage);
    MQTTpublishIntStruct(Alarm, bmsInternal);

    subtopic = "battery/warnings/";
    MQTTpublishIntStruct(Warning, highCurrentDischarge);
    MQTTpublishIntStruct(Warning, highCurrentCharge);
    MQTTpublishIntStruct(Warning, lowTemperature);
    MQTTpublishIntStruct(Warning, highTemperature);
    MQTTpublishIntStruct(Warning, lowVoltage);
    MQTTpublishIntStruct(Warning, highVoltage);
    MQTTpublishIntStruct(Warning, bmsInternal);

    subtopic = "battery/charging/";
    MQTTpublishIntStruct(ChargeDischargeManagementInfo, chargeEnable);
    MQTTpublishIntStruct(ChargeDischargeManagementInfo, dischargeEnable);
    MQTTpublishIntStruct(ChargeDischargeManagementInfo, chargeImmediately);
    MQTTpublishIntStruct(ChargeDischargeManagementInfo, chargeImmediately1);
    MQTTpublishIntStruct(ChargeDischargeManagementInfo, fullChargeRequest);

    subtopic = "battery/voltages/";
    MQTTpublish(cellMinVoltage, 3);
    MQTTpublish(cellMaxVoltage, 3);
    MQTTpublish(cellDiffVoltage, 0);
    if (numberOfCells > 15)
        numberOfCells = 15;
    _last.CellVoltages = reinterpret_cast<float*>(realloc(_last.CellVoltages, numberOfCells * sizeof(float)));
    for (int i = 0; i < numberOfCells; i++) {
        if (!cBattery.UpdatesOnly || CellVoltages[i] != _last.CellVoltages[i]) {
            MqttSettings.publish(subtopic + "cell" + String(i + 1), String(CellVoltages[i], 3));
            _last.CellVoltages[i] = CellVoltages[i];
        }
    }

    subtopic = "battery/temperatures/";
    MQTTpublish(averageBMSTemperature, 1);

    subtopic = "battery/temperatures/group";
    if (numberOfTemperatures > 5)
        numberOfTemperatures = 5;
    _last.GroupedCellsTemperatures = reinterpret_cast<float*>(realloc(_last.GroupedCellsTemperatures, (numberOfTemperatures - 1) * sizeof(float)));
    for (int i = 0; i < numberOfTemperatures - 1; i++) {
        if (!cBattery.UpdatesOnly || GroupedCellsTemperatures[i] != _last.GroupedCellsTemperatures[i]) {
            MqttSettings.publish(subtopic + String(i + 1), String(GroupedCellsTemperatures[i], 1));
            _last.GroupedCellsTemperatures[i] = GroupedCellsTemperatures[i];
        }
    }
}
#endif

#ifdef USE_DALYBMS_CONTROLLER
void DalyBmsBatteryStats::mqttPublish() /* const */
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    String subtopic = "battery/";
    MQTTpublishInt(batteryCycles);
    MQTTpublish(voltage, 2);
    MQTTpublish(current, 2);
    MQTTpublish(power, 3);
    MQTTpublish(remainingCapacity, 3);
    MQTTpublish(ratedCapacity, 3);
/*
    subtopic = "battery/parameters/";
    MQTTpublishStruct(SystemParameters, chargeCurrentLimit, 2)
    MQTTpublishStruct(SystemParameters, dischargeCurrentLimit, 2);
    MQTTpublishStruct(SystemParameters, cellHighVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, cellLowVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, cellUnderVoltageLimit, 2);
    MQTTpublishStruct(SystemParameters, chargeHighTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, chargeLowTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, dischargeHighTemperatureLimit, 1);
    MQTTpublishStruct(SystemParameters, dischargeLowTemperatureLimit, 1);
*/
    subtopic = "battery/alarms/";
    MQTTpublishIntStruct(Alarm, overCurrentDischarge);
    MQTTpublishIntStruct(Alarm, overCurrentCharge);
    MQTTpublishIntStruct(Alarm, underTemperature);
    MQTTpublishIntStruct(Alarm, overTemperature);
    MQTTpublishIntStruct(Alarm, underVoltage);
    MQTTpublishIntStruct(Alarm, overVoltage);
    MQTTpublishIntStruct(Alarm, bmsInternal);

    subtopic = "battery/warnings/";
    MQTTpublishIntStruct(Warning, highCurrentDischarge);
    MQTTpublishIntStruct(Warning, highCurrentCharge);
    MQTTpublishIntStruct(Warning, lowTemperature);
    MQTTpublishIntStruct(Warning, highTemperature);
    MQTTpublishIntStruct(Warning, lowVoltage);
    MQTTpublishIntStruct(Warning, highVoltage);
    MQTTpublishIntStruct(Warning, bmsInternal);

    subtopic = "battery/charging/";
    MQTTpublishInt(chargingMosEnabled);
    MQTTpublishInt(dischargingMosEnabled);
    MQTTpublishInt(chargeImmediately);

    subtopic = "battery/voltages/";
    MQTTpublish(minCellVoltage, 3);
    MQTTpublish(maxCellVoltage, 3);
    MQTTpublish(cellDiffVoltage, 0);
    if (_cellsNumber > 15) _cellsNumber = 15;
    _last._cellVoltage = reinterpret_cast<float*>(realloc(_last._cellVoltage, _cellsNumber * sizeof(float)));
    for (int i = 0; i < _cellsNumber; i++) {
        if (!cBattery.UpdatesOnly || _cellVoltage[i] != _last._cellVoltage[i]) {
            MqttSettings.publish(subtopic + "cell" + String(i + 1), String(_cellVoltage[i], 3));
            _last._cellVoltage[i] = _cellVoltage[i];
        }
    }

    subtopic = "battery/temperatures/";
    MQTTpublish(averageBMSTemperature, 1);

    subtopic = "battery/temperatures/group";
    if (_tempsNumber > 5)
        _tempsNumber = 5;
    _last._temperature = reinterpret_cast<int*>(realloc(_last._temperature, _tempsNumber * sizeof(int)));
    for (int i = 0; i < _tempsNumber; i++) {
        if (!cBattery.UpdatesOnly || _temperature[i] != _last._temperature[i]) {
            MqttSettings.publish(subtopic + String(i + 1), String(_temperature[i], 1));
            _last._temperature[i] = _temperature[i];
        }
    }
}
/*
void DalyBmsBatteryStats::updateFrom(DalyBms::DataPointContainer const& dp)
{
    _dataPoints.updateFrom(dp);

    using Label = DalyBms::DataPointLabel;

    _manufacturer = "DalyBMS";
    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        _manufacturer = oProductId->c_str();
        auto pos = oProductId->rfind("Daly");
        if (pos != std::string::npos) {
            _manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = _dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        _SoC = *oSoCValue;
        auto oSoCDataPoint = _dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        _lastUpdateSoC = oSoCDataPoint->getTimestamp();
    }

    _lastUpdate = millis();
}
*/
#endif

#ifdef USE_JKBMS_CONTROLLER
void JkBmsBatteryStats::mqttPublish()  /* const */
{
    BatteryStats::mqttPublish();

    using Label = JkBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::ModificationPassword, // sensitive data
        Label::BatterySoCPercent // already published by base class
    };

    Mqtt_CONFIG_T& configMqtt = Configuration.get().Mqtt;

    // publish all topics every minute, unless the retain flag is enabled
    bool fullPublish = millis() - _lastFullMqttPublish > 60 * 1000;
    fullPublish &= !configMqtt.Retain;
/*
    // regularly publish all topics regardless of whether or not their value changed
    bool neverFullyPublished = _lastFullMqttPublish == 0;
    bool intervalElapsed = _lastFullMqttPublish + getMqttFullPublishIntervalMs() < millis();
    bool fullPublish = neverFullyPublished || intervalElapsed;
*/

    for (auto iter = _dataPoints.cbegin(); iter != _dataPoints.cend(); ++iter) {
        // skip data points that did not change since last published
        if (!fullPublish && iter->second.getTimestamp() < _lastMqttPublish) {
            continue;
        }

        auto skipMatch = std::find(mqttSkip.begin(), mqttSkip.end(), iter->first);
        if (skipMatch != mqttSkip.end()) {
            continue;
        }

        String topic((std::string("battery/") + iter->second.getLabelText()).c_str());
        MqttSettings.publish(topic, iter->second.getValueText().c_str());
    }

    auto oCellVoltages = _dataPoints.get<Label::CellsMilliVolt>();
    if (oCellVoltages.has_value() && (fullPublish || _cellVoltageTimestamp > _lastMqttPublish)) {
        unsigned idx = 1;
        for (auto iter = oCellVoltages->cbegin(); iter != oCellVoltages->cend(); ++iter) {
            String topic("battery/Cell");
            topic += String(idx);
            topic += "MilliVolt";

            MqttSettings.publish(topic, String(iter->second));

            ++idx;
        }

        MqttSettings.publish("battery/CellMinMilliVolt", String(_cellMinMilliVolt));
        MqttSettings.publish("battery/CellAvgMilliVolt", String(_cellAvgMilliVolt));
        MqttSettings.publish("battery/CellMaxMilliVolt", String(_cellMaxMilliVolt));
        MqttSettings.publish("battery/CellDiffMilliVolt", String(_cellMaxMilliVolt - _cellMinMilliVolt));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        for (auto iter = JkBms::AlarmBitTexts.begin(); iter != JkBms::AlarmBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oAlarms & static_cast<uint16_t>(bit)) ? "1" : "0";
            MqttSettings.publish(String("battery/alarms/") + iter->second.data(), value);
        }
    }

    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        for (auto iter = JkBms::StatusBitTexts.begin(); iter != JkBms::StatusBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oStatus & static_cast<uint16_t>(bit)) ? "1" : "0";
            MqttSettings.publish(String("battery/status/") + iter->second.data(), value);
        }
    }

    _lastMqttPublish = millis();
    if (fullPublish) {
        _lastFullMqttPublish = _lastMqttPublish;
    }
}

void JkBmsBatteryStats::updateFrom(JkBms::DataPointContainer const& dp)
{
    using Label = JkBms::DataPointLabel;

    _manufacturer = "JKBMS";
    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        _manufacturer = oProductId->c_str();
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            _manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = _dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        _SoC = *oSoCValue;
        auto oSoCDataPoint = _dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        _lastUpdateSoC = oSoCDataPoint->getTimestamp();
    }

    _dataPoints.updateFrom(dp);

    auto oCellVoltages = _dataPoints.get<Label::CellsMilliVolt>();
    if (oCellVoltages.has_value()) {
        for (auto iter = oCellVoltages->cbegin(); iter != oCellVoltages->cend(); ++iter) {
            if (iter == oCellVoltages->cbegin()) {
                _cellMinMilliVolt = _cellAvgMilliVolt = _cellMaxMilliVolt = iter->second;
                continue;
            }
            _cellMinMilliVolt = std::min(_cellMinMilliVolt, iter->second);
            _cellAvgMilliVolt = (_cellAvgMilliVolt + iter->second) / 2;
            _cellMaxMilliVolt = std::max(_cellMaxMilliVolt, iter->second);
        }
        _cellVoltageTimestamp = millis();
    }

    _lastUpdate = millis();
}
#endif

#ifdef USE_VICTRON_SMART_SHUNT
void VictronSmartShuntStats::updateFrom(VeDirectShuntController::veShuntStruct const& shuntData)
{
    _SoC = shuntData.SOC / 10;
    _voltage = shuntData.V;
    _current = shuntData.I;
    _modelName = shuntData.getPidAsString().data();
    _chargeCycles = shuntData.H4;
    _timeToGo = shuntData.TTG / 60;
    _chargedEnergy = shuntData.H18 / 100;
    _dischargedEnergy = shuntData.H17 / 100;
    _manufacturer = "Victron " + _modelName;
    _temperature = shuntData.T;
    _tempPresent = shuntData.tempPresent;

    // shuntData.AR is a bitfield, so we need to check each bit individually
    _alarmLowVoltage = shuntData.AR & 1;
    _alarmHighVoltage = shuntData.AR & 2;
    _alarmLowSOC = shuntData.AR & 4;
    _alarmLowTemperature = shuntData.AR & 32;
    _alarmHighTemperature = shuntData.AR & 64;

    _lastUpdate = VeDirectShunt.getLastUpdate();
    _lastUpdateSoC = VeDirectShunt.getLastUpdate();
}

void VictronSmartShuntStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "voltage", _voltage, "V", 2);
    addLiveViewValue(root, "current", _current, "A", 1);
    addLiveViewValue(root, "chargeCycles", _chargeCycles, "", 0);
    addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "KWh", 1);
    addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "KWh", 1);
    if (_tempPresent) {
        addLiveViewValue(root, "temperature", _temperature, "°C", 0);
    }

    addLiveViewAlarm(root, "lowVoltage", _alarmLowVoltage);
    addLiveViewAlarm(root, "highVoltage", _alarmHighVoltage);
    addLiveViewAlarm(root, "lowSOC", _alarmLowSOC);
    addLiveViewAlarm(root, "lowTemperature", _alarmLowTemperature);
    addLiveViewAlarm(root, "highTemperature", _alarmHighTemperature);
}

void VictronSmartShuntStats::mqttPublish() /* const */
{
    BatteryStats::mqttPublish();

    MqttSettings.publish("battery/voltage", String(_voltage));
    MqttSettings.publish("battery/current", String(_current));
    MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
}
#endif
