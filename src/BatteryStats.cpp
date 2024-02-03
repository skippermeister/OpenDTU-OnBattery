// SPDX-License-Identifier: GPL-2.0-or-later
#include "BatteryStats.h"
#include "Configuration.h"
#include "JkBmsDataPoints.h"
#include "MqttSettings.h"

template <typename T>
void BatteryStats::addLiveViewValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision) const
{
    auto jsonValue = root["values"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void BatteryStats::addLiveViewParameter(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision) const
{
    auto jsonValue = root["parameters"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void BatteryStats::addLiveViewText(JsonVariant& root, std::string const& name,
    std::string const& text) const
{
    root["values"][name] = text;
}

void BatteryStats::addLiveViewWarning(JsonVariant& root, std::string const& name,
    bool warning) const
{
    if (!warning) {
        return;
    }
    root["issues"][name] = 1;
}

void BatteryStats::addLiveViewAlarm(JsonVariant& root, std::string const& name,
    bool alarm) const
{
    if (!alarm) {
        return;
    }
    root["issues"][name] = 2;
}

void BatteryStats::addLiveViewCellVoltage(JsonVariant& root, uint8_t index, float value, uint8_t precision) const
{
    auto jsonValue = root["cell"]["voltage"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "V";
    jsonValue["d"] = precision;
}

void BatteryStats::addLiveViewCellBalance(JsonVariant& root, std::string const& name,
    float value, std::string const& unit, uint8_t precision) const
{
    auto jsonValue = root["cell"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void BatteryStats::addLiveViewTempSensor(JsonVariant& root, uint8_t index, float value, uint8_t precision) const
{
    auto jsonValue = root["tempSensor"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "°C";
    jsonValue["d"] = precision;
}

void BatteryStats::getLiveViewData(JsonVariant& root) const
{
    root["manufacturer"] = manufacturer;
    root["device_name"] = deviceName;
    root["data_age"] = getAgeSeconds();

    addLiveViewValue(root, "SoC", SoC, "%", 0);
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

bool PylontechRS485BatteryStats::isChargeTemperatureValid() const
{
    const Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    return (minCellTemperature >= max(SystemParameters.chargeLowTemperatureLimit, static_cast<float>(cBattery.MinChargeTemperature)))
        && (maxCellTemperature <= min(SystemParameters.chargeHighTemperatureLimit, static_cast<float>(cBattery.MaxChargeTemperature)));
};

bool PylontechRS485BatteryStats::isDischargeTemperatureValid() const
{
    const Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    return (minCellTemperature >= max(SystemParameters.dischargeLowTemperatureLimit, static_cast<float>(cBattery.MinDischargeTemperature)))
        && (maxCellTemperature <= min(SystemParameters.dischargeHighTemperatureLimit, static_cast<float>(cBattery.MaxDischargeTemperature)));
};

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
    BatteryStats::getLiveViewData(root);

    using Label = DalyBms::DataPointLabel;

    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        addLiveViewValue(root, "voltage", static_cast<float>(*oVoltage) / 1000, "V", 2);
    }

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        addLiveViewValue(root, "current", static_cast<float>(*oCurrent) / 1000, "A", 2);
    }

    auto oTemperature = _dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperature.has_value()) {
        addLiveViewValue(root, "temperature", *oTemperature, "°C", 0);
    }
}
#endif

void BatteryStats::mqttPublish() // const
{
    String subtopic = "battery/";
    MqttSettings.publish(subtopic + "manufacturer", manufacturer);
    MqttSettings.publish(subtopic + "dataAge", String(getAgeSeconds()));
    MqttSettings.publish(subtopic + "stateOfCharge", String(SoC));
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
void PylontechCanBatteryStats::mqttPublish() // const
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

void PylontechRS485BatteryStats::mqttPublish() // const
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

#ifdef USE_DALYBMS_CONTROLLER
void DalyBmsBatteryStats::mqttPublish() // const
{
    BatteryStats::mqttPublish();

    using Label = DalyBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::ModificationPassword, // sensitive data
        Label::BatterySoCPercent // already published by base class
    };

    Mqtt_CONFIG_T& configMqtt = Configuration.get().Mqtt;

    // publish all topics every minute, unless the retain flag is enabled
    bool fullPublish = millis() - _lastFullMqttPublish > 60 * 1000;
    fullPublish &= !configMqtt.Retain;

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

    _lastMqttPublish = millis();
    if (fullPublish) {
        _lastFullMqttPublish = _lastMqttPublish;
    }
}

void DalyBmsBatteryStats::updateFrom(DalyBms::DataPointContainer const& dp)
{
    _dataPoints.updateFrom(dp);

    using Label = DalyBms::DataPointLabel;

    manufacturer = "DalyBMS";
    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        manufacturer = oProductId->c_str();
        auto pos = oProductId->rfind("Daly");
        if (pos != std::string::npos) {
            manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = _dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        SoC = *oSoCValue;
        auto oSoCDataPoint = _dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        lastUpdateSoC = oSoCDataPoint->getTimestamp();
    }

    lastUpdate = millis();
}
#endif

#ifdef USE_JKBMS_CONTROLLER
void JkBmsBatteryStats::mqttPublish() // const
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

    manufacturer = "JKBMS";
    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        manufacturer = oProductId->c_str();
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = _dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        SoC = *oSoCValue;
        auto oSoCDataPoint = _dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        lastUpdateSoC = oSoCDataPoint->getTimestamp();
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

    lastUpdate = millis();
}
#endif

#ifdef USE_VICTRON_SMART_SHUNT
void VictronSmartShuntStats::updateFrom(VeDirectShuntController::veShuntStruct const& shuntData)
{
    SoC = shuntData.SOC / 10;
    _voltage = shuntData.V;
    _current = shuntData.I;
    _modelName = shuntData.getPidAsString().data();
    _chargeCycles = shuntData.H4;
    _timeToGo = shuntData.TTG / 60;
    _chargedEnergy = shuntData.H18 / 100;
    _dischargedEnergy = shuntData.H17 / 100;
    manufacturer = "Victron " + _modelName;
    _temperature = shuntData.T;
    _tempPresent = shuntData.tempPresent;

    // shuntData.AR is a bitfield, so we need to check each bit individually
    _alarmLowVoltage = shuntData.AR & 1;
    _alarmHighVoltage = shuntData.AR & 2;
    _alarmLowSOC = shuntData.AR & 4;
    _alarmLowTemperature = shuntData.AR & 32;
    _alarmHighTemperature = shuntData.AR & 64;

    lastUpdate = VeDirectShunt.getLastUpdate();
    lastUpdateSoC = VeDirectShunt.getLastUpdate();
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

void VictronSmartShuntStats::mqttPublish()
{ // const {
    BatteryStats::mqttPublish();

    MqttSettings.publish("battery/voltage", String(_voltage));
    MqttSettings.publish("battery/current", String(_current));
    MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
}
#endif
