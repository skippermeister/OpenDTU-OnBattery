// SPDX-License-Identifier: GPL-2.0-or-later
#include "BatteryStats.h"
#include "Configuration.h"
#include "DalyBmsController.h"
#include "JkBmsDataPoints.h"
#include "JbdBmsDataPoints.h"
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
static void addLiveViewPackValue(JsonObject& root, std::string const& name,
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

template <typename T>
static void addLiveViewPackParameter(JsonObject& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["parameters"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

static void addLiveViewText(JsonVariant& root, std::string const& name,
    std::string const& text, bool translate = true)
{
    auto jsonValue = root["values"][name];
    jsonValue["value"] = text;
    jsonValue["translate"] = translate;
}

#ifdef USE_PYTES_CAN_RECEIVER
static void addLiveViewPackText(JsonObject& root, std::string const& name,
    std::string const& text, bool translate = true)
{
    auto jsonValue = root["values"][name];
    jsonValue["value"] = text;
    jsonValue["translate"] = translate;
}
#endif

static void addLiveViewWarning(JsonVariant& root, std::string const& name,
    bool warning)
{
    if (warning) { root["issues"][name] = 1; }
}

static void addLiveViewAlarm(JsonVariant& root, std::string const& name,
    bool alarm)
{
    if (alarm) { root["issues"][name] = 2; }
}

/*
static void addLiveViewCellVoltage(JsonVariant& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["cell"]["voltage"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "V";
    jsonValue["d"] = precision;
}
*/
static void addLiveViewPackCellVoltage(JsonObject& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["cell"]["voltage"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "V";
    jsonValue["d"] = precision;
}
/*
static void addLiveViewCellBalance(JsonVariant& root, std::string const& name,
    float value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["cell"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}
*/
static void addLiveViewPackCellBalance(JsonObject& root, std::string const& name,
    float value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["cell"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}
/*
static void addLiveViewTempSensor(JsonVariant& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["tempSensor"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "°C";
    jsonValue["d"] = precision;
}
*/
static void addLiveViewPackTempSensor(JsonObject& root, uint8_t index, float value, uint8_t precision)
{
    auto jsonValue = root["tempSensor"][index];
    jsonValue["v"] = value;
    jsonValue["u"] = "°C";
    jsonValue["d"] = precision;
}

void BatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const
{
}

bool BatteryStats::updateAvailable(uint32_t since) const
{
    if (_lastUpdate == 0) { return false; } // no data at all processed yet

    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
    return (_lastUpdate - since) < halfOfAllMillis;
}

void BatteryStats::getLiveViewData(JsonVariant& root) const
{
    root["manufacturer"] = _manufacturer;
    if (!_fwversion.isEmpty()) {
        root["fwversion"] = _fwversion;
    }
    if (!_hwversion.isEmpty()) {
        root["hwversion"] = _hwversion;
    }
    root["data_age"] = getAgeSeconds();

    addLiveViewValue(root, "SoC", _SoC, "%", _socPrecision);
    addLiveViewValue(root, "voltage", _voltage, "V", 3);
    addLiveViewValue(root, "current", _current, "A", _currentPrecision);
}

#define ISSUE(token, value) addLiveView##token(root, #value, token.value > 0);
#define ISSUEtotals(token, value) addLiveView##token(root, #value, totals.token.value > 0);

#ifdef USE_PYLONTECH_CAN_RECEIVER
void PylontechCanBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", chargeVoltage, "V", 1);
    addLiveViewValue(root, "chargeCurrentLimit", chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeVoltageLimit", dischargeVoltageLimit, "V", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", dischargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "stateOfHealth", stateOfHealth, "%", 0);
    addLiveViewValue(root, "temperature", temperature, "°C", 1);

    addLiveViewText(root, "chargeEnabled", (chargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (dischargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately", (chargeImmediately ? "yes" : "no"));
    addLiveViewValue(root, "modules", moduleCount, "", 0);

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
void PylontechRS485BatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t module) const
{
    packObject["moduleNumber"] = Pack[module].numberOfModule;
    packObject["moduleName"] = _number_of_packs == 1 ? "Single" : (module == 0 ? "Master" : String("Slave-") + String(module));

    packObject["device_name"] = Pack[module].deviceName;
    packObject["moduleSerialNumber"] = Pack[module].ModuleSerialNumber.moduleSerialNumber;
    packObject["software_version"] = Pack[module].softwareVersion;

    addLiveViewPackValue(packObject, "capacity", Pack[module].capacity, "Ah", 3);
    addLiveViewPackValue(packObject, "remainingCapacity", Pack[module].remainingCapacity, "Ah", 3);
    addLiveViewPackValue(packObject, "cycles", Pack[module].cycles, "", 0);
    addLiveViewPackValue(packObject, "current", Pack[module].current, "A", 1);
    addLiveViewPackValue(packObject, "power", Pack[module].power, "kW", 3);
    addLiveViewPackValue(packObject, "BMSTemperature", Pack[module].averageBMSTemperature, "°C", 1);
    //    addLiveViewPackValue(packObject, "CellTemperature", Pack[module].averageCellTemperature, "°C", 1);
    addLiveViewPackValue(packObject, "CellTemperature", Pack[module].maxCellTemperature, "°C", 1);
    addLiveViewPackValue(packObject, "chargeCurrentLimit", Pack[module].ChargeDischargeManagementInfo.chargeCurrentLimit, "A", 1);
    addLiveViewPackValue(packObject, "dischargeCurrentLimit", Pack[module].ChargeDischargeManagementInfo.dischargeCurrentLimit, "A", 1);
    addLiveViewPackValue(packObject, "maxChargeCurrentLimit", Pack[module].SystemParameters.chargeCurrentLimit, "A", 1);
    addLiveViewPackValue(packObject, "maxDischargeCurrentLimit", Pack[module].SystemParameters.dischargeCurrentLimit, "A", 1);

    addLiveViewPackParameter(packObject, "cellHighVoltageLimit", Pack[module].SystemParameters.cellHighVoltageLimit, "V", 3);
    addLiveViewPackParameter(packObject, "cellLowVoltageLimit", Pack[module].SystemParameters.cellLowVoltageLimit, "V", 3);
    addLiveViewPackParameter(packObject, "cellUnderVoltageLimit", Pack[module].SystemParameters.cellUnderVoltageLimit, "V", 3);
    addLiveViewPackParameter(packObject, "chargeHighTemperatureLimit", Pack[module].SystemParameters.chargeHighTemperatureLimit, "°C", 1);
    addLiveViewPackParameter(packObject, "chargeLowTemperatureLimit", Pack[module].SystemParameters.chargeLowTemperatureLimit, "°C", 1);
    addLiveViewPackParameter(packObject, "dischargeHighTemperatureLimit", Pack[module].SystemParameters.dischargeHighTemperatureLimit, "°C", 1);
    addLiveViewPackParameter(packObject, "dischargeLowTemperatureLimit", Pack[module].SystemParameters.dischargeLowTemperatureLimit, "°C", 1);

    addLiveViewPackCellBalance(packObject, "cellMinVoltage", Pack[module].cellMinVoltage, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellMaxVoltage", Pack[module].cellMaxVoltage, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellDiffVoltage", Pack[module].cellDiffVoltage, "mV", 0);

    for (int i = 0; i < Pack[module].numberOfCells; i++) {
        addLiveViewPackCellVoltage(packObject, i, Pack[module].CellVoltages[i], 3);
    }
    for (int i = 0; i < Pack[module].numberOfTemperatures - 1; i++) {
        addLiveViewPackTempSensor(packObject, i, Pack[module].GroupedCellsTemperatures[i], 1);
    }
}

void PylontechRS485BatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "capacity", totals.capacity, "Ah", 3);
    addLiveViewValue(root, "remainingCapacity", totals.remainingCapacity, "Ah", 3);

    addLiveViewValue(root, "cycles", totals.cycles, "", 0);

    // angeforderte Ladespannung
    // addLiveViewValue(root, "chargeVoltage", totals.chargeVoltage, "V", 3);

    addLiveViewValue(root, "power", totals.power, "kW", 3);
    addLiveViewValue(root, "BMSTemperature", totals.averageBMSTemperature, "°C", 1);
    //    addLiveViewValue(root, "CellTemperature", totals.averageCellTemperature, "°C", 1);
    addLiveViewValue(root, "CellTemperature", totals.maxCellTemperature, "°C", 1);

    // Empfohlene Lade-/Enladespannung
    addLiveViewValue(root, "chargeVoltageLimit", totals.ChargeDischargeManagementInfo.chargeVoltageLimit, "V", 3);
    addLiveViewValue(root, "dischargeVoltageLimit", totals.ChargeDischargeManagementInfo.dischargeVoltageLimit, "V", 3);
    // Empfohlene Lade-/Enladesstrom
    addLiveViewValue(root, "chargeCurrentLimit", totals.ChargeDischargeManagementInfo.chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", totals.ChargeDischargeManagementInfo.dischargeCurrentLimit, "A", 1);

    // maximale Lade-/Enladestrom (90A@15sec)
    addLiveViewValue(root, "maxChargeCurrentLimit", totals.SystemParameters.chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "maxDischargeCurrentLimit", totals.SystemParameters.dischargeCurrentLimit, "A", 1);

    addLiveViewText(root, "chargeEnabled", (totals.ChargeDischargeManagementInfo.chargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (totals.ChargeDischargeManagementInfo.dischargeEnabled ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately1", (totals.ChargeDischargeManagementInfo.chargeImmediately1 ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately2", (totals.ChargeDischargeManagementInfo.chargeImmediately2 ? "yes" : "no"));
    addLiveViewText(root, "fullChargeRequest", (totals.ChargeDischargeManagementInfo.fullChargeRequest ? "yes" : "no"));

    // alarms and warnings go into the "Issues" card of the web application
    ISSUEtotals(Warning, highCurrentDischarge);
    ISSUEtotals(Alarm, overCurrentDischarge);

    ISSUEtotals(Warning, highCurrentCharge);
    ISSUEtotals(Alarm, overCurrentCharge);

    ISSUEtotals(Warning, lowTemperature);
    ISSUEtotals(Alarm, underTemperature);

    ISSUEtotals(Warning, highTemperature);
    ISSUEtotals(Alarm, overTemperature);

    ISSUEtotals(Warning, lowVoltage);
    ISSUEtotals(Alarm, underVoltage);

    ISSUEtotals(Warning, highVoltage);
    ISSUEtotals(Alarm, overVoltage);

    root["numberOfPacks"] = _number_of_packs;

    // only show the consolidated values in the main status card if we have more than one battery pack
    if (_number_of_packs > 1) {
        addLiveViewValue(root, "cellMinVoltage", totals.cellMinVoltage, "V", 3);
        addLiveViewValue(root, "cellMaxVoltage", totals.cellMaxVoltage, "V", 3);
        addLiveViewValue(root, "cellDiffVoltage", totals.cellDiffVoltage, "mV", 0);
    }
}
#endif

#ifdef USE_GOBEL_RS485_RECEIVER
void GobelRS485BatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t module) const
{
}

void GobelRS485BatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

}
#endif

#ifdef USE_PYTES_CAN_RECEIVER
void PytesBatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t module) const
{
    packObject["moduleNumber"] = 1;
    packObject["moduleName"] = "";

    packObject["device_name"] = "";

    packObject["software_version"] = "";

    packObject["moduleSerialNumber"] = _serial;

    addLiveViewPackCellBalance(packObject, "cellMinVoltage", static_cast<float>(_cellMinMilliVolt)/1000, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt)/1000, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);

    addLiveViewPackValue(packObject, "cellMinTemperature", _cellMinTemperature, "°C", 0);
    addLiveViewPackValue(packObject, "cellMaxTemperature", _cellMaxTemperature, "°C", 0);

    addLiveViewPackText(packObject, "cellMinVoltageName", _cellMinVoltageName.c_str(), false);
    addLiveViewPackText(packObject, "cellMaxVoltageName", _cellMaxVoltageName.c_str(), false);
    addLiveViewPackText(packObject, "cellMinTemperatureName", _cellMinTemperatureName.c_str(), false);
    addLiveViewPackText(packObject, "cellMaxTemperatureName", _cellMaxTemperatureName.c_str(), false);
}

void PytesBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", _chargeVoltageLimit, "V", 1);
    addLiveViewValue(root, "chargeCurrentLimit", _chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeVoltageLimit", _dischargeVoltageLimit, "V", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", _dischargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "stateOfHealth", _stateOfHealth, "%", 0);
    if (_chargeCycles != -1) {
        addLiveViewValue(root, "cycles", _chargeCycles, "", 0);
    }
    addLiveViewValue(root, "temperature", _temperature, "°C", 1);

    addLiveViewValue(root, "capacity", _totalCapacity, "Ah", _capacityPrecision);
    addLiveViewValue(root, "remainingCapacity", _availableCapacity, "Ah", _capacityPrecision);

    if (_chargedEnergy != -1) {
        addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "kWh", 1);
    }

    if (_dischargedEnergy != -1) {
        addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "kWh", 1);
    }

    if (_balance != -1) {
        addLiveViewTextValue(root, "balancingActive", (_balance?"yes":"no"));
    }
    addLiveViewTextValue(root, "chargeImmediately", (_chargeImmediately?"yes":"no"));

    addLiveViewValue(root, "online", _moduleCountOnline, "Module", 0);
    addLiveViewValue(root, "offline", _moduleCountOffline, "Module", 0);
    addLiveViewValue(root, "chargeEnabled", _moduleCountBlockingCharge, "Module", 0);
    addLiveViewValue(root, "dischargeEnabled", _moduleCountBlockingDischarge, "Module", 0);

    // alarms and warnings go into the "Issues" card of the web application
    ISSUE(Warning, highCurrentDischarge);
    ISSUE(Alarm, overCurrentDischarge);

    ISSUE(Warning, highCurrentCharge);
    ISSUE(Alarm, overCurrentCharge);

    ISSUE(Warning, lowVoltage);
    ISSUE(Alarm, underVoltage);

    ISSUE(Warning, highVoltage);
    ISSUE(Alarm, overVoltage);

    ISSUE(Warning, lowTemperature);
    ISSUE(Alarm, underTemperature);

    ISSUE(Warning, highTemperature);
    ISSUE(Alarm, overTemperature);

    ISSUE(Warning, lowTemperatureCharge);
    ISSUE(Alarm, underTemperatureCharge);

    ISSUE(Warning, highTemperatureCharge);
    ISSUE(Alarm, overTemperatureCharge);

    ISSUE(Warning, bmsInternal);
    ISSUE(Alarm, bmsInternal);
}
#endif

#ifdef USE_SBS_CAN_RECEIVER
void SBSBatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t module) const
{
    packObject["moduleNumber"] = 1;
    packObject["moduleName"] = "";

    packObject["device_name"] = "";

    packObject["software_version"] = "";

    packObject["moduleSerialNumber"] = _serial;
/*
    addLiveViewPackCellBalance(packObject, "cellMinVoltage", static_cast<float>(_cellMinMilliVolt)/1000, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt)/1000, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);

    addLiveViewPackValue(packObject, "cellMinTemperature", _cellMinTemperature, "V", 3);
    addLiveViewPackValue(packObject, "cellMaxTemperature", _cellMaxTemperature, "V", 3);

    addLiveViewPackText(packObject, "cellMinVoltageName", _cellMinVoltageName.c_str(), false);
    addLiveViewPackText(packObject, "cellMaxVoltageName", _cellMaxVoltageName.c_str(), false);
    addLiveViewPackText(packObject, "cellMinTemperatureName", _cellMinTemperatureName.c_str(), false);
    addLiveViewPackText(packObject, "cellMaxTemperatureName", _cellMaxTemperatureName.c_str(), false);
*/
}

void SBSBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", _chargeVoltage, "V", 1);
    addLiveViewValue(root, "chargeCurrentLimit", _chargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", _dischargeCurrentLimit, "A", 1);
    addLiveViewValue(root, "stateOfHealth", _stateOfHealth, "%", 0);
    addLiveViewValue(root, "current", _current, "A", 1);
    addLiveViewValue(root, "temperature", _temperature, "°C", 1);
    addLiveViewText(root, "chargeEnabled", (_chargeEnabled?"yes":"no"));
    addLiveViewText(root, "dischargeEnabled", (_dischargeEnabled?"yes":"no"));

    // alarms and warnings go into the "Issues" card of the web application
    ISSUE(Warning, highCurrentDischarge);
    ISSUE(Warning, highCurrentCharge);
    ISSUE(Alarm, underVoltage);
    ISSUE(Alarm, overVoltage);
    ISSUE(Alarm, underTemperature);
    ISSUE(Alarm, overTemperature);
}
#endif

#ifdef USE_JKBMS_CONTROLLER
void JkBmsBatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t module) const
{
    using Label = JkBms::DataPointLabel;

    packObject["moduleNumber"] = 1;
    packObject["moduleName"] = "";

    packObject["device_name"] = "";

    auto oBmsSoftwareVersion = _dataPoints.get<Label::BmsSoftwareVersion>();
    if (oBmsSoftwareVersion.has_value()) packObject["software_version"] = *oBmsSoftwareVersion;
    else packObject["software_version"] = "";

    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) packObject["moduleSerialNumber"] = *oProductId;
    else packObject["moduleSerialNumber"] = "";

    auto oTemperatureOne = _dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperatureOne.has_value()) {
        //addLiveViewInSection(root, "cells", "batOneTemp", *oTemperatureOne, "°C", 0);
//        addLiveViewPackValue(packObject, "batOneTemp", *oTemperatureOne, "°C", 0);
        addLiveViewPackTempSensor(packObject, 0, *oTemperatureOne, 0);
    }

    auto oTemperatureTwo = _dataPoints.get<Label::BatteryTempTwoCelsius>();
    if (oTemperatureTwo.has_value()) {
        //addLiveViewInSection(root, "cells", "batTwoTemp", *oTemperatureTwo, "°C", 0);
//        addLiveViewPackValue(packObject, "batTwoTemp", *oTemperatureTwo, "°C", 0);
        addLiveViewPackTempSensor(packObject, 1, *oTemperatureTwo, 0);
    }

    if (_cellVoltageTimestamp > 0) {
        //addLiveViewInSection(root, "cells", "cellMinVoltage", static_cast<float>(_cellMinMilliVolt) / 1000, "V", 3);
        //addLiveViewInSection(root, "cells", "cellAvgVoltage", static_cast<float>(_cellAvgMilliVolt) / 1000, "V", 3);
        //addLiveViewInSection(root, "cells", "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt) / 1000, "V", 3);
        //addLiveViewInSection(root, "cells", "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);

        //addLiveViewPackValue(packObject, "cellMinVoltage", static_cast<float>(_cellMinMilliVolt) / 1000, "V", 3);
        //addLiveViewPackValue(packObject, "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt) / 1000, "V", 3);
        //addLiveViewPackValue(packObject, "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);

        addLiveViewPackCellBalance(packObject, "cellMinVoltage", static_cast<float>(_cellMinMilliVolt) / 1000, "V", 3);
        addLiveViewPackCellBalance(packObject, "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt) / 1000, "V", 3);
        addLiveViewPackCellBalance(packObject, "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);

        auto oCellsMilliVolt = _dataPoints.get<Label::CellsMilliVolt>();
        if (oCellsMilliVolt.has_value()) {
            JkBms::tCells voltages = *oCellsMilliVolt;
            for (int8_t i=0; i<voltages.size(); i++) {
                addLiveViewPackCellVoltage(packObject, i, static_cast<float>(voltages[i+1]) / 1000, 3);
            }
        }
    }

    auto oCellOvervoltageThresholdMilliVolt = _dataPoints.get<Label::CellOvervoltageThresholdMilliVolt>();
    auto oCellUndervoltageThresholdMilliVolt = _dataPoints.get<Label::CellUndervoltageThresholdMilliVolt>();
    auto oChargeHighTempThresholdCelsius = _dataPoints.get<Label::ChargeHighTempThresholdCelsius>();
    auto oChargeLowTempThresholdCelsius = _dataPoints.get<Label::ChargeLowTempThresholdCelsius>();
    auto oDischargeHighTempThresholdCelsius = _dataPoints.get<Label::DischargeHighTempThresholdCelsius>();
    auto oDischargeLowTempThresholdCelsius = _dataPoints.get<Label::DischargeLowTempThresholdCelsius>();
    if (oCellOvervoltageThresholdMilliVolt.has_value())
        addLiveViewPackParameter(packObject, "cellHighVoltageLimit", static_cast<float>(*oCellOvervoltageThresholdMilliVolt) / 1000, "V", 3);
    if (oCellUndervoltageThresholdMilliVolt.has_value())
        addLiveViewPackParameter(packObject, "cellUnderVoltageLimit", static_cast<float>(*oCellUndervoltageThresholdMilliVolt) / 1000, "V", 3);
    if (oChargeHighTempThresholdCelsius.has_value())
        addLiveViewPackParameter(packObject, "chargeHighTemperatureLimit", *oChargeHighTempThresholdCelsius, "°C", 0);
    if (oChargeLowTempThresholdCelsius.has_value())
        addLiveViewPackParameter(packObject, "chargeLowTemperatureLimit", *oChargeLowTempThresholdCelsius, "°C", 0);
    if (oDischargeHighTempThresholdCelsius.has_value())
        addLiveViewPackParameter(packObject, "dischargeHighTemperatureLimit",*oDischargeHighTempThresholdCelsius, "°C", 0);
    if (oDischargeLowTempThresholdCelsius.has_value())
        addLiveViewPackParameter(packObject, "dischargeLowTemperatureLimit", *oDischargeLowTempThresholdCelsius, "°C", 0);
}

void JkBmsBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    using Label = JkBms::DataPointLabel;

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value() && oCurrent.has_value()) {
        auto current = static_cast<float>(*oCurrent) / 1000;
        auto voltage = static_cast<float>(*oVoltage) / 1000;
        addLiveViewValue(root, "power", current * voltage , "W", 2);
    }

    auto oActualBatteryCapacityAmpHours = _dataPoints.get<Label::ActualBatteryCapacityAmpHours>();
    if (oActualBatteryCapacityAmpHours.has_value()) {
        addLiveViewValue(root, "remainingCapacity", *oActualBatteryCapacityAmpHours, "Ah", 0);
    }

    auto oBatteryCapacitySettingAmpHours = _dataPoints.get<Label::BatteryCapacitySettingAmpHours>();
    if (oBatteryCapacitySettingAmpHours.has_value()) {
        addLiveViewValue(root, "capacity", *oBatteryCapacitySettingAmpHours, "Ah", 0);
    }
    auto oBatteryCycleCapacity = _dataPoints.get<Label::BatteryCycleCapacity>();
    if (oBatteryCycleCapacity.has_value()) {
        addLiveViewValue(root, "cycleCapacity", *oBatteryCycleCapacity, "Ah", 0);
    }

    auto oTemperatureBms = _dataPoints.get<Label::BmsTempCelsius>();
    if (oTemperatureBms.has_value()) {
        addLiveViewValue(root, "BMSTemperature", *oTemperatureBms, "°C", 0);
    }

    auto oBatteryCellAmount = _dataPoints.get<Label::BatteryCellAmount>();
    if (oBatteryCellAmount.has_value()) {
        addLiveViewValue(root, "numberOfCells", static_cast<float>(*oBatteryCellAmount), "", 0);
    }
    auto oBatteryCycles = _dataPoints.get<Label::BatteryCycles>();
    if (oBatteryCycles.has_value()) {
        addLiveViewValue(root, "cycles", static_cast<float>(*oBatteryCycles), "", 0);
    }

    // labels BatteryChargeEnabled, BatteryDischargeEnabled, and
    // BalancingEnabled refer to the user setting. we want to show the
    // actual MOSFETs' state which control whether charging and discharging
    // is possible and whether the BMS is currently balancing cells.
    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        auto chargeEnabled = *oStatus & static_cast<uint16_t>(Bits::ChargingActive);
        addLiveViewText(root, "chargeEnabled", (chargeEnabled ? "yes" : "no"));
        auto dischargeEnabled = *oStatus & static_cast<uint16_t>(Bits::DischargingActive);
        addLiveViewText(root, "dischargeEnabled", (dischargeEnabled ? "yes" : "no"));

        auto balancingActive = *oStatus & static_cast<uint16_t>(Bits::BalancingActive);
        //addLiveViewTextInSection(root, "cells", "balancingActive", (balancingActive ? "yes" : "no"));
        addLiveViewText(root, "balancingActive", (balancingActive ? "yes" : "no"));
    }
    addLiveViewText(root, "chargeImmediately1", (ChargeDischargeManagementInfo.chargeImmediately1 ? "yes" : "no"));

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
#define ISSUE_JKBMS(t, x)                                           \
    auto x = *oAlarms & static_cast<uint16_t>(JkBms::AlarmBits::x); \
    addLiveView##t(root, "JkBmsIssue" #x, x > 0);

        ISSUE_JKBMS(Warning, LowCapacity);
        ISSUE_JKBMS(Alarm, BmsOvertemperature);
        ISSUE_JKBMS(Alarm, ChargingOvervoltage);
        ISSUE_JKBMS(Alarm, DischargeUndervoltage);
        ISSUE_JKBMS(Alarm, BatteryOvertemperature);
        ISSUE_JKBMS(Alarm, ChargingOvercurrent);
        ISSUE_JKBMS(Alarm, DischargeOvercurrent);
        ISSUE_JKBMS(Alarm, CellVoltageDifference);
        ISSUE_JKBMS(Alarm, BatteryBoxOvertemperature);
        ISSUE_JKBMS(Alarm, BatteryUndertemperature);
        ISSUE_JKBMS(Alarm, CellOvervoltage);
        ISSUE_JKBMS(Alarm, CellUndervoltage);
        ISSUE_JKBMS(Alarm, AProtect);
        ISSUE_JKBMS(Alarm, BProtect);
#undef ISSUE_JKBMS
    }

    root["numberOfPacks"] = 1;
}
#endif

#ifdef USE_DALYBMS_CONTROLLER
static void addLiveViewFailureStatus(JsonVariant& root, std::string const& name,
    bool alarm)
{
    if (!alarm) {
        return;
    }
    root["issues"][name] = 2;
}

void DalyBmsBatteryStats::generatePackCommonJsonResponse(JsonObject& packObject, const uint8_t m) const
{
    packObject["moduleNumber"] = 1;
    packObject["moduleName"] = "";

    packObject["device_name"] = "";

    packObject["software_version"] = String(_bmsSWversion);

    packObject["moduleSerialNumber"] = "";

    addLiveViewPackCellBalance(packObject, "cellMinVoltage", minCellVoltage, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellMaxVoltage", maxCellVoltage, "V", 3);
    addLiveViewPackCellBalance(packObject, "cellDiffVoltage", cellDiffVoltage, "mV", 0);

    for (int i = 0; i < min(_cellsNumber, static_cast<uint8_t>(DALY_MAX_NUMBER_CELLS)); i++) {
        addLiveViewPackCellVoltage(packObject, i, _cellVoltage[i], 3);
    }
    for (int i = 0; i < min(_tempsNumber, static_cast<uint8_t>(DALY_MAX_NUMBER_TEMP_SENSORS)); i++) {
        addLiveViewPackTempSensor(packObject, i, _temperature[i], 1);
    }

    addLiveViewPackParameter(packObject, "cellHighVoltageLimit", AlarmValues.maxCellVoltage, "V", 3);
    addLiveViewPackParameter(packObject, "cellLowVoltageLimit", WarningValues.minCellVoltage, "V", 3);
    addLiveViewPackParameter(packObject, "cellUnderVoltageLimit", AlarmValues.minCellVoltage, "V", 3);
}

void DalyBmsBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    addLiveViewValue(root, "BatteryLevel", batteryLevel, "%", 1);

    addLiveViewValue(root, "capacity", ratedCapacity, "Ah", 3);
    addLiveViewValue(root, "remainingCapacity", remainingCapacity, "Ah", 3);

    addLiveViewValue(root, "cycles", batteryCycles, "", 0);
    addLiveViewValue(root, "gatherVoltage", gatherVoltage, "V", 1);
    addLiveViewValue(root, "power", power, "kW", 3);
    addLiveViewValue(root, "currentSamplingResistance", currentSamplingResistance, "mΩ", 0);

    addLiveViewText(root, "chargeEnabled", (chargingMosEnabled ? "yes" : "no"));
    addLiveViewText(root, "dischargeEnabled", (dischargingMosEnabled ? "yes" : "no"));
    addLiveViewText(root, "chargeImmediately1", chargeImmediately1 ? "yes" : "no");
    addLiveViewText(root, "chargeImmediately2", chargeImmediately2 ? "yes" : "no");
    addLiveViewText(root, "cellBalanceActive", cellBalanceActive ? "yes" : "no");

    // Empfohlene Lade-/Enladespannung
    addLiveViewValue(root, "chargeVoltageLimit", WarningValues.maxPackVoltage, "V", 3);
    addLiveViewValue(root, "dischargeVoltageLimit", WarningValues.minPackVoltage, "V", 3);
    // Empfohlene Lade-/Enladesstrom
    addLiveViewValue(root, "chargeCurrentLimit", WarningValues.maxPackChargeCurrent, "A", 1);
    addLiveViewValue(root, "dischargeCurrentLimit", WarningValues.maxPackDischargeCurrent, "A", 1);

    // maximale Lade-/Enladestrom (90A@15sec)
    addLiveViewValue(root, "maxChargeCurrentLimit", AlarmValues.maxPackChargeCurrent, "A", 1);
    addLiveViewValue(root, "maxDischargeCurrentLimit", AlarmValues.maxPackDischargeCurrent, "A", 1);

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

    root["numberOfPacks"] = 1;
}
#endif

#ifdef USE_JBDBMS_CONTROLLER
void JbdBmsBatteryStats::getJsonData(JsonVariant& root, bool verbose) const
{
    BatteryStats::getLiveViewData(root);

    using Label = JbdBms::DataPointLabel;

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value() && oCurrent.has_value()) {
        auto current = static_cast<float>(*oCurrent) / 1000;
        auto voltage = static_cast<float>(*oVoltage) / 1000;
        addLiveViewValue(root, "power", current * voltage , "W", 2);
    }

    auto oBatteryChargeEnabled = _dataPoints.get<Label::BatteryChargeEnabled>();
    if (oBatteryChargeEnabled.has_value()) {
        addLiveViewTextValue(root, "chargeEnabled", (*oBatteryChargeEnabled?"yes":"no"));
    }

    auto oBatteryDischargeEnabled = _dataPoints.get<Label::BatteryDischargeEnabled>();
    if (oBatteryDischargeEnabled.has_value()) {
        addLiveViewTextValue(root, "dischargeEnabled", (*oBatteryDischargeEnabled?"yes":"no"));
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
        addLiveViewInSection(root, "cells", "cellMinVoltage", static_cast<float>(_cellMinMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellAvgVoltage", static_cast<float>(_cellAvgMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);
    }

    auto oBalancingEnabled = _dataPoints.get<Label::BalancingEnabled>();
    if (oBalancingEnabled.has_value()) {
        addLiveViewTextInSection(root, "cells", "balancingActive", (*oBalancingEnabled?"yes":"no"));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
#define ISSUE(t, x) \
        auto x = *oAlarms & static_cast<uint16_t>(JbdBms::AlarmBits::x); \
        addLiveView##t(root, "JbdBmsIssue"#x, x > 0);

        //ISSUE(Warning, LowCapacity);
        ISSUE(Alarm, CellOverVoltage);
        ISSUE(Alarm, CellUnderVoltage);
        ISSUE(Alarm, PackOverVoltage);
        ISSUE(Alarm, PackUnderVoltage);
        ISSUE(Alarm, ChargingOverTemperature);
        ISSUE(Alarm, ChargingLowTemperature);
        ISSUE(Alarm, DischargingOverTemperature);
        ISSUE(Alarm, DischargingLowTemperature);
        ISSUE(Alarm, ChargingOverCurrent);
        ISSUE(Alarm, DischargeOverCurrent);
        ISSUE(Alarm, ShortCircuit);
        ISSUE(Alarm, IcFrontEndError);
        ISSUE(Alarm, MosSotwareLock);
        ISSUE(Alarm, Reserved1);
        ISSUE(Alarm, Reserved2);
        ISSUE(Alarm, Reserved3);
#undef ISSUE
    }
}
#endif

void BatteryStats::mqttLoop()
{
    auto const& config = Configuration.get();

    if (!MqttSettings.getConnected()
        || (millis() - _lastMqttPublish) < (config.Mqtt.PublishInterval * 1000)) {
        return;
    }

    mqttPublish();

    _lastMqttPublish = millis();
}

uint32_t BatteryStats::getMqttFullPublishIntervalMs() const
{
    auto const& config = Configuration.get();

    // this is the default interval, see mqttLoop(). mqttPublish()
    // implementations in derived classes may choose to publish some values
    // with a lower frequency and hence implement this method with a different
    // return value.
    return config.Mqtt.PublishInterval * 1000;
}

void BatteryStats::mqttPublish() /* const */
{
    String subtopic = "battery/";
    MqttSettings.publish(subtopic + "manufacturer", _manufacturer);
    MqttSettings.publish(subtopic + "dataAge", String(getAgeSeconds()));
    if (isSoCValid()) {
        MqttSettings.publish(subtopic + "stateOfCharge", String(_SoC));
    }
    if (isVoltageValid()) {
        MqttSettings.publish(subtopic + "voltage", String(_voltage));
    }
    if (isCurrentValid()) {
        MqttSettings.publish(subtopic + "current", String(_current));
    }
}

#define MQTTpublish(value, digits)                     \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = value, digits));
#define MQTTpublishTotals(value, digits)                            \
    if (!cBattery.UpdatesOnly || totals.value != _lastTotals.value) \
        MqttSettings.publish(subtopic + #value, String(_lastTotals.value = totals.value, digits));
#define MQTTpublishPack(module, value, digits)                                  \
    if (!cBattery.UpdatesOnly || Pack[module].value != _lastPack[module].value) \
        MqttSettings.publish(subtopic + #value, String(_lastPack[module].value = Pack[module].value, digits));
#define MQTTpublishStruct(str, value, digits)                  \
    if (!cBattery.UpdatesOnly || str.value != _last.str.value) \
        MqttSettings.publish(subtopic + #value, String(_last.str.value = str.value, digits));
#define MQTTpublishTotalsStruct(str, value, digits)                         \
    if (!cBattery.UpdatesOnly || totals.str.value != _lastTotals.str.value) \
        MqttSettings.publish(subtopic + #value, String(_lastTotals.str.value = totals.str.value, digits));
#define MQTTpublishPackStruct(module, str, value, digits)                               \
    if (!cBattery.UpdatesOnly || Pack[module].str.value != _lastPack[module].str.value) \
        MqttSettings.publish(subtopic + #value, String(_lastPack[module].str.value = Pack[module].str.value, digits));
#define MQTTpublishInt(value)                          \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, String(_last.value = value));
#define MQTTpublishTotalsInt(value)                                 \
    if (!cBattery.UpdatesOnly || totals.value != _lastTotals.value) \
        MqttSettings.publish(subtopic + #value, String(_lastTotals.value = totals.value));
#define MQTTpublishPackInt(module, value)                                       \
    if (!cBattery.UpdatesOnly || Pack[module].value != _lastPack[module].value) \
        MqttSettings.publish(subtopic + #value, String(_lastPack[module].value = Pack[module].value));
#define MQTTpublishIntStruct(str, value)                       \
    if (!cBattery.UpdatesOnly || str.value != _last.str.value) \
        MqttSettings.publish(subtopic + #value, String(_last.str.value = str.value));
#define MQTTpublishTotalsIntStruct(str, value)                              \
    if (!cBattery.UpdatesOnly || totals.str.value != _lastTotals.str.value) \
        MqttSettings.publish(subtopic + #value, String(_lastTotals.str.value = totals.str.value));
#define MQTTpublishPackIntStruct(module, str, value)                                    \
    if (!cBattery.UpdatesOnly || Pack[module].str.value != _lastPack[module].str.value) \
        MqttSettings.publish(subtopic + #value, String(_lastPack[module].str.value = Pack[module].str.value));
#define MQTTpublishHex(value, str)                               \
    if (!cBattery.UpdatesOnly || str##value != _last.str##value) \
        MqttSettings.publish(subtopic + #value, String(_last.str##value = str##value, HEX));
#define MQTTpublishString(value)                       \
    if (!cBattery.UpdatesOnly || value != _last.value) \
        MqttSettings.publish(subtopic + #value, _last.value = value);
#define MQTTpublishPackString(module, value)                                    \
    if (!cBattery.UpdatesOnly || Pack[module].value != _lastPack[module].value) \
        MqttSettings.publish(subtopic + #value, _lastPack[module].value = Pack[module].value);

#ifdef USE_PYLONTECH_CAN_RECEIVER
void PylontechCanBatteryStats::mqttPublish() /* const */
{
    auto const& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    String subtopic = "battery/settings/";
    MQTTpublish(chargeVoltage, 2);
    MQTTpublish(chargeCurrentLimit, 2);
    MQTTpublish(dischargeCurrentLimit, 2);
    MQTTpublish(dischargeVoltage, 2);

    subtopic = "battery/";
    MQTTpublishInt(stateOfHealth);
//    MQTTpublish(current, 2);
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
    auto const& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    const String topic = "battery/";
    String subtopic = topic;
//    MQTTpublishTotals(current, 2);
    MQTTpublishTotals(power, 3);
    MQTTpublishTotals(remainingCapacity, 3);
    MQTTpublishTotals(capacity, 3);
    MQTTpublishTotalsInt(cycles);

    subtopic = topic + "settings/";
    //    MQTTpublishTotals(chargeVoltage, 2);
    MQTTpublishTotalsStruct(ChargeDischargeManagementInfo, chargeCurrentLimit, 2)
    MQTTpublishTotalsStruct(ChargeDischargeManagementInfo, dischargeCurrentLimit, 2);

    subtopic = topic + "parameters/";
    MQTTpublishTotalsStruct(SystemParameters, chargeCurrentLimit, 2)
    MQTTpublishTotalsStruct(SystemParameters, dischargeCurrentLimit, 2);
    MQTTpublishTotalsStruct(SystemParameters, chargeHighTemperatureLimit, 1);
    MQTTpublishTotalsStruct(SystemParameters, chargeLowTemperatureLimit, 1);
    MQTTpublishTotalsStruct(SystemParameters, dischargeHighTemperatureLimit, 1);
    MQTTpublishTotalsStruct(SystemParameters, dischargeLowTemperatureLimit, 1);

    subtopic = topic + "alarms/";
    MQTTpublishTotalsIntStruct(Alarm, overCurrentDischarge);
    MQTTpublishTotalsIntStruct(Alarm, overCurrentCharge);
    MQTTpublishTotalsIntStruct(Alarm, underTemperature);
    MQTTpublishTotalsIntStruct(Alarm, overTemperature);
    MQTTpublishTotalsIntStruct(Alarm, underVoltage);
    MQTTpublishTotalsIntStruct(Alarm, overVoltage);
    MQTTpublishTotalsIntStruct(Alarm, bmsInternal);
    subtopic = topic + "warnings/";
    MQTTpublishTotalsIntStruct(Warning, highCurrentDischarge);
    MQTTpublishTotalsIntStruct(Warning, highCurrentCharge);
    MQTTpublishTotalsIntStruct(Warning, lowTemperature);
    MQTTpublishTotalsIntStruct(Warning, highTemperature);
    MQTTpublishTotalsIntStruct(Warning, lowVoltage);
    MQTTpublishTotalsIntStruct(Warning, highVoltage);
    MQTTpublishTotalsIntStruct(Warning, bmsInternal);

    subtopic = topic + "charging/";
    MQTTpublishTotalsIntStruct(ChargeDischargeManagementInfo, chargeEnabled);
    MQTTpublishTotalsIntStruct(ChargeDischargeManagementInfo, dischargeEnabled);
    MQTTpublishTotalsIntStruct(ChargeDischargeManagementInfo, chargeImmediately1);
    MQTTpublishTotalsIntStruct(ChargeDischargeManagementInfo, chargeImmediately2);
    MQTTpublishTotalsIntStruct(ChargeDischargeManagementInfo, fullChargeRequest);

    subtopic = topic + "cellVoltages/";
    MQTTpublishTotals(cellMinVoltage, 3);
    MQTTpublishTotals(cellMaxVoltage, 3);
    MQTTpublishTotals(cellDiffVoltage, 0);

    subtopic = topic + "temperatures/";
    MQTTpublishTotals(averageBMSTemperature, 1);
    MQTTpublishTotals(minCellTemperature, 1);
    MQTTpublishTotals(maxCellTemperature, 1);

    for (uint8_t module = 0; module < _number_of_packs; module++) {
        const String moduleTopic = topic + String(module) + "/";
        subtopic = moduleTopic;
        MQTTpublishPackString(module, deviceName);
        MQTTpublishPackString(module, softwareVersion);
        MQTTpublishPackInt(module, cycles);

        String subtopic = moduleTopic + "settings/";
        // MQTTpublishPack(module, chargeVoltage, 2);
        MQTTpublishPackStruct(module, ChargeDischargeManagementInfo, chargeCurrentLimit, 2)
        MQTTpublishPackStruct(module, ChargeDischargeManagementInfo, dischargeCurrentLimit, 2);

        subtopic = moduleTopic + "charging/";
        MQTTpublishPackIntStruct(module, ChargeDischargeManagementInfo, chargeEnabled);
        MQTTpublishPackIntStruct(module, ChargeDischargeManagementInfo, dischargeEnabled);
        MQTTpublishPackIntStruct(module, ChargeDischargeManagementInfo, chargeImmediately1);
        MQTTpublishPackIntStruct(module, ChargeDischargeManagementInfo, chargeImmediately2);
        MQTTpublishPackIntStruct(module, ChargeDischargeManagementInfo, fullChargeRequest);

        subtopic = moduleTopic + "parameters/";
        MQTTpublishPackStruct(module, SystemParameters, chargeCurrentLimit, 2)
        MQTTpublishPackStruct(module, SystemParameters, dischargeCurrentLimit, 2);
        MQTTpublishPackStruct(module, SystemParameters, cellHighVoltageLimit, 2);
        MQTTpublishPackStruct(module, SystemParameters, cellLowVoltageLimit, 2);
        MQTTpublishPackStruct(module, SystemParameters, cellUnderVoltageLimit, 2);
        MQTTpublishPackStruct(module, SystemParameters, chargeHighTemperatureLimit, 1);
        MQTTpublishPackStruct(module, SystemParameters, chargeLowTemperatureLimit, 1);
        MQTTpublishPackStruct(module, SystemParameters, dischargeHighTemperatureLimit, 1);
        MQTTpublishPackStruct(module, SystemParameters, dischargeLowTemperatureLimit, 1);

        subtopic = moduleTopic + "alarms/";
        MQTTpublishPackIntStruct(module, Alarm, overCurrentDischarge);
        MQTTpublishPackIntStruct(module, Alarm, overCurrentCharge);
        MQTTpublishPackIntStruct(module, Alarm, underTemperature);
        MQTTpublishPackIntStruct(module, Alarm, overTemperature);
        MQTTpublishPackIntStruct(module, Alarm, underVoltage);
        MQTTpublishPackIntStruct(module, Alarm, overVoltage);
        MQTTpublishPackIntStruct(module, Alarm, bmsInternal);

        subtopic = moduleTopic + "warnings/";
        MQTTpublishPackIntStruct(module, Warning, highCurrentDischarge);
        MQTTpublishPackIntStruct(module, Warning, highCurrentCharge);
        MQTTpublishPackIntStruct(module, Warning, lowTemperature);
        MQTTpublishPackIntStruct(module, Warning, highTemperature);
        MQTTpublishPackIntStruct(module, Warning, lowVoltage);
        MQTTpublishPackIntStruct(module, Warning, highVoltage);
        MQTTpublishPackIntStruct(module, Warning, bmsInternal);

        subtopic = moduleTopic + "cellVoltages/";
        MQTTpublishPack(module, cellMinVoltage, 3);
        MQTTpublishPack(module, cellMaxVoltage, 3);
        MQTTpublishPack(module, cellDiffVoltage, 0);

        subtopic = subtopic + "cell";
        _lastPack[module].CellVoltages = reinterpret_cast<float*>(realloc(_lastPack[module].CellVoltages, Pack[module].numberOfCells * sizeof(float)));
        for (int i = 0; i < Pack[module].numberOfCells; i++) {
            if (!cBattery.UpdatesOnly || Pack[module].CellVoltages[i] != _lastPack[module].CellVoltages[i]) {
                MqttSettings.publish(subtopic + String(i + 1), String(Pack[module].CellVoltages[i], 3));
                _lastPack[module].CellVoltages[i] = Pack[module].CellVoltages[i];
            }
        }

        subtopic = moduleTopic + "temperatures/";
        MQTTpublishPack(0, averageBMSTemperature, 1);

        subtopic = subtopic + "group";
        _lastPack[module].GroupedCellsTemperatures = reinterpret_cast<float*>(realloc(_lastPack[module].GroupedCellsTemperatures, (Pack[module].numberOfTemperatures - 1) * sizeof(float)));
        for (int i = 0; i < Pack[module].numberOfTemperatures - 1; i++) {
            if (!cBattery.UpdatesOnly || Pack[module].GroupedCellsTemperatures[i] != _lastPack[module].GroupedCellsTemperatures[i]) {
                MqttSettings.publish(subtopic + String(i + 1), String(Pack[module].GroupedCellsTemperatures[i], 1));
                _lastPack[module].GroupedCellsTemperatures[i] = Pack[module].GroupedCellsTemperatures[i];
            }
        }
    }
}
#endif

#ifdef USE_GOBEL_RS485_RECEIVER
void GobelRS485BatteryStats::mqttPublish() /*const*/
{
    auto const& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    const String topic = "battery/";
    String subtopic = topic;
}
#endif

#ifdef USE_DALYBMS_CONTROLLER
void DalyBmsBatteryStats::mqttPublish() /* const */
{
    auto const& cBattery = Configuration.get().Battery;

    BatteryStats::mqttPublish();

    String subtopic = "battery/";
    MQTTpublishInt(batteryCycles);
//    MQTTpublish(current, 2);
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
    MQTTpublishInt(chargeImmediately1);
    MQTTpublishInt(chargeImmediately2);
    MQTTpublishInt(cellBalanceActive);

    subtopic = "battery/cellVoltages/";
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

    subtopic = subtopic + "group";
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
#endif

#ifdef USE_PYTES_CAN_RECEIVER
void PytesBatteryStats::mqttPublish() /* const */
{
    BatteryStats::mqttPublish();

    MqttSettings.publish("battery/settings/chargeVoltage", String(_chargeVoltageLimit));
    MqttSettings.publish("battery/settings/chargeCurrentLimitation", String(_chargeCurrentLimit));
    MqttSettings.publish("battery/settings/dischargeCurrentLimitation", String(_dischargeCurrentLimit));
    MqttSettings.publish("battery/settings/dischargeVoltageLimitation", String(_dischargeVoltageLimit));

    MqttSettings.publish("battery/stateOfHealth", String(_stateOfHealth));
    if (_chargeCycles != -1) {
        MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    }
    if (_balance != -1) {
        MqttSettings.publish("battery/balancingActive", String(_balance ? 1 : 0));
    }
    MqttSettings.publish("battery/temperature", String(_temperature));

    if (_chargedEnergy != -1) {
        MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    }

    if (_dischargedEnergy != -1) {
        MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
    }

    MqttSettings.publish("battery/capacity", String(_totalCapacity));
    MqttSettings.publish("battery/availableCapacity", String(_availableCapacity));

    MqttSettings.publish("battery/CellMinMilliVolt", String(_cellMinMilliVolt));
    MqttSettings.publish("battery/CellMaxMilliVolt", String(_cellMaxMilliVolt));
    MqttSettings.publish("battery/CellDiffMilliVolt", String(_cellMaxMilliVolt - _cellMinMilliVolt));
    MqttSettings.publish("battery/CellMinTemperature", String(_cellMinTemperature));
    MqttSettings.publish("battery/CellMaxTemperature", String(_cellMaxTemperature));
    MqttSettings.publish("battery/CellMinVoltageName", String(_cellMinVoltageName));
    MqttSettings.publish("battery/CellMaxVoltageName", String(_cellMaxVoltageName));
    MqttSettings.publish("battery/CellMinTemperatureName", String(_cellMinTemperatureName));
    MqttSettings.publish("battery/CellMaxTemperatureName", String(_cellMaxTemperatureName));

    MqttSettings.publish("battery/modulesOnline", String(_moduleCountOnline));
    MqttSettings.publish("battery/modulesOffline", String(_moduleCountOffline));
    MqttSettings.publish("battery/modulesBlockingCharge", String(_moduleCountBlockingCharge));
    MqttSettings.publish("battery/modulesBlockingDischarge", String(_moduleCountBlockingDischarge));

    MqttSettings.publish("battery/alarm/overCurrentDischarge", String(Alarm.overCurrentDischarge));
    MqttSettings.publish("battery/alarm/overCurrentCharge", String(Alarm.overCurrentCharge));
    MqttSettings.publish("battery/alarm/underVoltage", String(Alarm.underVoltage));
    MqttSettings.publish("battery/alarm/overVoltage", String(Alarm.overVoltage));
    MqttSettings.publish("battery/alarm/underTemperature", String(Alarm.underTemperature));
    MqttSettings.publish("battery/alarm/overTemperature", String(Alarm.overTemperature));
    MqttSettings.publish("battery/alarm/underTemperatureCharge", String(Alarm.underTemperatureCharge));
    MqttSettings.publish("battery/alarm/overTemperatureCharge", String(Alarm.overTemperatureCharge));
    MqttSettings.publish("battery/alarm/bmsInternal", String(Alarm.bmsInternal));
    MqttSettings.publish("battery/alarm/cellImbalance", String(Alarm.cellImbalance));

    MqttSettings.publish("battery/warning/highCurrentDischarge", String(Warning.highCurrentDischarge));
    MqttSettings.publish("battery/warning/highCurrentCharge", String(Warning.highCurrentCharge));
    MqttSettings.publish("battery/warning/lowVoltage", String(Warning.lowVoltage));
    MqttSettings.publish("battery/warning/highVoltage", String(Warning.highVoltage));
    MqttSettings.publish("battery/warning/lowTemperature", String(Warning.lowTemperature));
    MqttSettings.publish("battery/warning/highTemperature", String(Warning.highTemperature));
    MqttSettings.publish("battery/warning/lowTemperatureCharge", String(Warning.lowTemperatureCharge));
    MqttSettings.publish("battery/warning/highTemperatureCharge", String(Warning.highTemperatureCharge));
    MqttSettings.publish("battery/warning/bmsInternal", String(Warning.bmsInternal));
    MqttSettings.publish("battery/warning/cellImbalance", String(Warning.cellImbalance));
    MqttSettings.publish("battery/charging/chargeImmediately", String(_chargeImmediately));
}
#endif

#ifdef USE_SBS_CAN_RECEIVER
void SBSBatteryStats::mqttPublish() // const
{
    BatteryStats::mqttPublish();

    MqttSettings.publish("battery/settings/chargeVoltage", String(_chargeVoltage));
    MqttSettings.publish("battery/settings/chargeCurrentLimitation", String(_chargeCurrentLimit));
    MqttSettings.publish("battery/settings/dischargeCurrentLimitation", String(_dischargeCurrentLimit));
    MqttSettings.publish("battery/stateOfHealth", String(_stateOfHealth));
    MqttSettings.publish("battery/current", String(_current));
    MqttSettings.publish("battery/temperature", String(_temperature));
    MqttSettings.publish("battery/alarm/underVoltage", String(Alarm.underVoltage));
    MqttSettings.publish("battery/alarm/overVoltage", String(Alarm.overVoltage));
    MqttSettings.publish("battery/alarm/bmsInternal", String(Alarm.bmsInternal));
    MqttSettings.publish("battery/warning/highCurrentDischarge", String(Warning.highCurrentDischarge));
    MqttSettings.publish("battery/warning/highCurrentCharge", String(Warning.highCurrentCharge));
    MqttSettings.publish("battery/charging/chargeEnabled", String(_chargeEnabled));
    MqttSettings.publish("battery/charging/dischargeEnabled", String(_dischargeEnabled));
}
#endif

#ifdef USE_JBDBMS_CONTROLLER
void JbdBmsBatteryStats::mqttPublish() const
{
    BatteryStats::mqttPublish();

    using Label = JbdBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::BatterySoCPercent // already published by base class
        // NOTE that voltage is also published by the base class, however, we
        // previously published it only from here using the respective topic.
        // to avoid a breaking change, we publish the value again using the
        // "old" topic.
    };

    // regularly publish all topics regardless of whether or not their value changed
    bool neverFullyPublished = _lastFullMqttPublish == 0;
    bool intervalElapsed = _lastFullMqttPublish + getMqttFullPublishIntervalMs() < millis();
    bool fullPublish = neverFullyPublished || intervalElapsed;

    for (auto iter = _dataPoints.cbegin(); iter != _dataPoints.cend(); ++iter) {
        // skip data points that did not change since last published
        if (!fullPublish && iter->second.getTimestamp() < _lastMqttPublish) { continue; }

        auto skipMatch = std::find(mqttSkip.begin(), mqttSkip.end(), iter->first);
        if (skipMatch != mqttSkip.end()) { continue; }

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
        for (auto iter = JbdBms::AlarmBitTexts.begin(); iter != JbdBms::AlarmBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oAlarms & static_cast<uint16_t>(bit))?"1":"0";
            MqttSettings.publish(String("battery/alarms/") + iter->second.data(), value);
        }
    }

    _lastMqttPublish = millis();
    if (fullPublish) { _lastFullMqttPublish = _lastMqttPublish; }
}

void JbdBmsBatteryStats::updateFrom(JbdBms::DataPointContainer const& dp)
{
    using Label = JbdBms::DataPointLabel;

    _manufacturer = "JBDBMS";

    auto oSoCValue = dp.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        auto oSoCDataPoint = dp.getDataPointFor<Label::BatterySoCPercent>();
        BatteryStats::setSoC(*oSoCValue, 0/*precision*/,
                oSoCDataPoint->getTimestamp());
    }

    auto oVoltage = dp.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        auto oVoltageDataPoint = dp.getDataPointFor<Label::BatteryVoltageMilliVolt>();
        BatteryStats::setVoltage(static_cast<float>(*oVoltage) / 1000,
                oVoltageDataPoint->getTimestamp());
    }

    auto oCurrent = dp.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        auto oCurrentDataPoint = dp.getDataPointFor<Label::BatteryCurrentMilliAmps>();
        BatteryStats::setCurrent(static_cast<float>(*oCurrent) / 1000, 2/*precision*/,
                oCurrentDataPoint->getTimestamp());
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

    auto oSoftwareVersion = _dataPoints.get<Label::BmsSoftwareVersion>();
    if (oSoftwareVersion.has_value()) {
        _fwversion = oSoftwareVersion->c_str();
    }

    auto oHardwareVersion = _dataPoints.get<Label::BmsHardwareVersion>();
    if (oHardwareVersion.has_value()) {
        _hwversion = oHardwareVersion->c_str();
    }

    _lastUpdate = millis();
}
#endif

#ifdef USE_JKBMS_CONTROLLER
void JkBmsBatteryStats::mqttPublish() /* const */
{
    BatteryStats::mqttPublish();

    using Label = JkBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::ModificationPassword, // sensitive data
        Label::BatterySoCPercent // already published by base class
        // NOTE that voltage is also published by the base class, however, we
        // previously published it only from here using the respective topic.
        // to avoid a breaking change, we publish the value again using the
        // "old" topic.
    };

    // regularly publish all topics regardless of whether or not their value changed
    bool neverFullyPublished = _lastFullMqttPublish == 0;
    bool intervalElapsed = _lastFullMqttPublish + getMqttFullPublishIntervalMs() < millis();
    bool fullPublish = neverFullyPublished || intervalElapsed;

    for (auto iter = _dataPoints.cbegin(); iter != _dataPoints.cend(); ++iter) {
        // skip data points that did not change since last published
        if (!fullPublish && iter->second.getTimestamp() < _lastMqttPublish) { continue; }

        auto skipMatch = std::find(mqttSkip.begin(), mqttSkip.end(), iter->first);
        if (skipMatch != mqttSkip.end()) { continue; }

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
    if (fullPublish) { _lastFullMqttPublish = _lastMqttPublish; }
}

void JkBmsBatteryStats::updateFrom(JkBms::DataPointContainer const& dp)
{
    using Label = JkBms::DataPointLabel;

    _manufacturer = "JKBMS";
    auto oProductId = dp.get<Label::ProductId>();
    if (oProductId.has_value()) {
        // the first twelve chars are expected to be the "User Private Data"
        // setting (see smartphone app). the remainder is expected be the BMS
        // name, which can be changed at will using the smartphone app. so
        // there is not always a "JK" in this string. if there is, we still cut
        // the string there to avoid possible regressions.
        _manufacturer = oProductId->substr(12).c_str();
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            _manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = dp.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        auto oSoCDataPoint = dp.getDataPointFor<Label::BatterySoCPercent>();
        BatteryStats::setSoC(*oSoCValue, 0 /*precision*/,
            oSoCDataPoint->getTimestamp());
    }

    auto oVoltage = dp.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        auto oVoltageDataPoint = dp.getDataPointFor<Label::BatteryVoltageMilliVolt>();
        BatteryStats::setVoltage(static_cast<float>(*oVoltage) / 1000,
            oVoltageDataPoint->getTimestamp());
    }

    auto oCurrent = dp.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        auto oCurrentDataPoint = dp.getDataPointFor<Label::BatteryCurrentMilliAmps>();
        BatteryStats::setCurrent(static_cast<float>(*oCurrent) / 1000, 2/*precision*/,
                oCurrentDataPoint->getTimestamp());
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

    auto oVersion = _dataPoints.get<Label::BmsSoftwareVersion>();
    if (oVersion.has_value()) {
        // raw: "11.XW_S11.262H_"
        //   => Hardware "V11.XW" (displayed in Android app)
        //   => Software "V11.262H" (displayed in Android app)
        auto first = oVersion->find('_');
        if (first != std::string::npos) {
            _hwversion = oVersion->substr(0, first).c_str();

            auto second = oVersion->find('_', first + 1);

            // the 'S' seems to be merely an indicator for "software"?
            if (oVersion->at(first + 1) == 'S') { first++; }

            _fwversion = oVersion->substr(first + 1, second - first - 1).c_str();
        }
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        using Bits = JkBms::AlarmBits;
        Alarm.overVoltage = *oAlarms & static_cast<uint16_t>(Bits::ChargingOvervoltage);
        Alarm.underTemperature = *oAlarms & static_cast<uint16_t>(Bits::BatteryUndertemperature);
        Alarm.overTemperature = *oAlarms & static_cast<uint16_t>(Bits::BatteryOvertemperature);
        Warning.highCurrentCharge = *oAlarms & static_cast<uint16_t>(Bits::ChargingOvercurrent);
        Alarm.overCurrentCharge = *oAlarms & static_cast<uint16_t>(Bits::ChargingOvercurrent);
        ChargeDischargeManagementInfo.chargeImmediately1 = *oAlarms & static_cast<uint16_t>(Bits::LowCapacity);
    }

    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        ChargeDischargeManagementInfo.chargeEnabled = *oStatus & static_cast<uint16_t>(Bits::ChargingActive);
        ChargeDischargeManagementInfo.dischargeEnabled = *oStatus & static_cast<uint16_t>(Bits::DischargingActive);
    }

    auto oTemperatureBms = _dataPoints.get<Label::BmsTempCelsius>();
    auto oTemperatureOne = _dataPoints.get<Label::BatteryTempOneCelsius>();
    auto oTemperatureTwo = _dataPoints.get<Label::BatteryTempTwoCelsius>();
    if (oTemperatureBms.has_value() || oTemperatureOne.has_value() || oTemperatureTwo.has_value()) {
        _minTemperature = 100;
        _maxTemperature = -100;
    }
    if (oTemperatureBms.has_value()) {
        _minTemperature = min(*oTemperatureBms, _minTemperature);
        _maxTemperature = max(*oTemperatureBms, _maxTemperature);
    }
    if (oTemperatureOne.has_value()) {
        _minTemperature = min(*oTemperatureOne, _minTemperature);
        _maxTemperature = max(*oTemperatureOne, _maxTemperature);
    }
    if (oTemperatureTwo.has_value()) {
        _minTemperature = min(*oTemperatureTwo, _minTemperature);
        _maxTemperature = max(*oTemperatureTwo, _maxTemperature);
    }

    _lastUpdate = millis();
}
#endif

#ifdef USE_VICTRON_SMART_SHUNT
void VictronSmartShuntStats::updateFrom(VeDirectShuntController::data_t const& shuntData) {
    BatteryStats::setVoltage(shuntData.batteryVoltage_V_mV / 1000.0, millis());
    BatteryStats::setSoC(static_cast<float>(shuntData.SOC) / 10, 1 /*precision*/, millis());
    BatteryStats::setCurrent(static_cast<float>(shuntData.batteryCurrent_I_mA) / 1000, 2/*precision*/, millis());
    _fwversion = shuntData.getFwVersionFormatted();

    _chargeCycles = shuntData.H4;
    _timeToGo = shuntData.TTG / 60;
    _chargedEnergy = shuntData.H18 / 100;
    _dischargedEnergy = shuntData.H17 / 100;
    _manufacturer = String("Victron ") + shuntData.getPidAsString().data();
    _temperature = shuntData.T;
    _tempPresent = shuntData.tempPresent;
    _midpointVoltage = static_cast<float>(shuntData.VM) / 1000;
    _midpointDeviation = static_cast<float>(shuntData.DM) / 10;
    _instantaneousPower = shuntData.P;
    _consumedAmpHours = static_cast<float>(shuntData.CE) / 1000;
    _lastFullCharge = shuntData.H9 / 60;
    // shuntData.AR is a bitfield, so we need to check each bit individually
    _alarmLowVoltage = shuntData.alarmReason_AR & 1;
    _alarmHighVoltage = shuntData.alarmReason_AR & 2;
    _alarmLowSOC = shuntData.alarmReason_AR & 4;
    _alarmLowTemperature = shuntData.alarmReason_AR & 32;
    _alarmHighTemperature = shuntData.alarmReason_AR & 64;

    _lastUpdate = VeDirectShunt.getLastUpdate();
}

void VictronSmartShuntStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "cycles", _chargeCycles, "", 0);
    addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "KWh", 1);
    addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "KWh", 1);
    addLiveViewValue(root, "power", _instantaneousPower, "W", 0);
    addLiveViewValue(root, "consumedAmpHours", _consumedAmpHours, "Ah", 3);
    addLiveViewValue(root, "midpointVoltage", _midpointVoltage, "V", 2);
    addLiveViewValue(root, "midpointDeviation", _midpointDeviation, "%", 1);
    addLiveViewValue(root, "lastFullCharge", _lastFullCharge, "min", 0);
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

    MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
    MqttSettings.publish("battery/instantaneousPower", String(_instantaneousPower));
    MqttSettings.publish("battery/consumedAmpHours", String(_consumedAmpHours));
    MqttSettings.publish("battery/lastFullCharge", String(_lastFullCharge));
    MqttSettings.publish("battery/midpointVoltage", String(_midpointVoltage));
    MqttSettings.publish("battery/midpointDeviation", String(_midpointDeviation));
}
#endif

#ifdef USE_MQTT_ZENDURE_BATTERY
void ZendureBatteryStats::update(JsonObjectConst props, unsigned int ms){
    auto soc = Utils::getJsonElement<float>(props, "electricLevel");
    if (soc.has_value() && (*soc >= 0 && *soc <= 100)){
        setSoC(*soc, 0/*precision*/, ms);
    }

    auto soc_max = Utils::getJsonElement<float>(props, "socSet");
    if (soc_max.has_value()){
        *soc_max /= 10;
        if (*soc_max >= 40 && *soc_max <= 100){
            _soc_max = *soc_max;
        }
    }

    auto soc_min = Utils::getJsonElement<float>(props, "minSoc");
    if (soc_min.has_value()){
        *soc_min /= 10;
        if (*soc_min >= 0 && *soc_min <= 60){
            _soc_min = *soc_min;
        }
    }

    auto input_limit = Utils::getJsonElement<uint16_t>(props, "inputLimit");
    if (input_limit.has_value()){
        _input_limit = *input_limit;
    }

    auto output_limit = Utils::getJsonElement<uint16_t>(props, "outputLimit");
    if (output_limit.has_value()){
        _output_limit = *output_limit;
    }

    auto inverse_max = Utils::getJsonElement<uint16_t>(props, "inverseMaxPower");
    if (inverse_max.has_value()){
        _inverse_max = *inverse_max;
    }

    auto pass_mode = Utils::getJsonElement<uint8_t>(props, "passMode");
    if (pass_mode.has_value()){
        _bypass_mode = *pass_mode;
    }

    auto pass_state = Utils::getJsonElement<uint8_t>(props, "pass");
    if (pass_state.has_value()){
        _bypass_state = static_cast<bool>(*pass_state);
    }

    auto batteries = Utils::getJsonElement<uint8_t>(props, "packNum");
    if (batteries.has_value() && batteries <= 4){
        _num_batteries = *batteries;
    }

    auto state = Utils::getJsonElement<uint8_t>(props, "packState");
    if (state.has_value()){
        _state = *state;
    }

    auto heat_state = Utils::getJsonElement<uint8_t>(props, "heatState");
    if (heat_state.has_value()){
        _heat_state = *heat_state;
    }

    auto auto_shutdown = Utils::getJsonElement<uint8_t>(props, "hubState");
    if (auto_shutdown.has_value()){
        _auto_shutdown = *auto_shutdown;
    }

    auto buzzer = Utils::getJsonElement<uint8_t>(props, "buzzerSwitch");
    if (buzzer.has_value()){
        _buzzer = *buzzer;
    }

    auto auto_recover = Utils::getJsonElement<uint8_t>(props, "autoRecover");
    if (auto_recover.has_value()){
        _auto_recover = static_cast<bool>(*auto_recover);
    }

    auto charge_power = Utils::getJsonElement<uint16_t>(props, "outputPackPower");
    if (charge_power.has_value()){
        _charge_power = *charge_power;
    }

    auto discharge_power = Utils::getJsonElement<uint16_t>(props, "packInputPower");
    if (discharge_power.has_value()){
        _discharge_power = *discharge_power;
    }

    auto output_power = Utils::getJsonElement<uint16_t>(props, "outputHomePower");
    if (output_power.has_value()){
        _output_power = *output_power;
    }

    auto input_power = Utils::getJsonElement<uint16_t>(props, "solarInputPower");
    if (input_power.has_value()){
        _input_power = *input_power;
    }

    auto solar_power_1 = Utils::getJsonElement<uint16_t>(props, "solarPower1");
    if (solar_power_1.has_value()){
        _solar_power_1 = *solar_power_1;
    }

    auto solar_power_2 = Utils::getJsonElement<uint16_t>(props, "solarPower2");
    if (solar_power_2.has_value()){
        _solar_power_2 = *solar_power_2;
    }

    float voltage = getVoltage();
    if (charge_power.has_value() && discharge_power.has_value() && voltage){
        setCurrent((*charge_power - *discharge_power) / voltage, 2, ms);
    }

    auto sw_version = Utils::getJsonElement<uint32_t>(props, "masterSoftVersion");
    if (sw_version.has_value()){
        _fwversion = String(*sw_version);
    }

    auto hw_version = Utils::getJsonElement<uint32_t>(props, "masterhaerVersion");
    if (hw_version.has_value()){
        _hwversion = String(*hw_version);
    } else {
        hw_version = Utils::getJsonElement<uint32_t>(props, "masterHaerVersion");
        if (hw_version.has_value()){
            _hwversion = String(*hw_version);
        }
    }

    auto outtime = Utils::getJsonElement<uint16_t>(props, "remainOutTime");
    if (outtime.has_value()){
        _remain_out_time = *outtime;
    }

    auto intime = Utils::getJsonElement<uint16_t>(props, "remainInputTime");
    if (intime.has_value()){
        _remain_in_time = *intime;
    }

    calculateEfficiency();
}

std::optional<ZendureBatteryStats::ZendurePackStats*> ZendureBatteryStats::getPackData(String serial) const {
    try
    {
        return _packData.at(serial);
    }
    catch(const std::out_of_range& ex)
    {
        return std::nullopt;
    }
}

void ZendureBatteryStats::updatePackData(String serial, JsonObjectConst packData, unsigned int ms){
    try
    {
        _packData.at(serial);
    }
    catch(const std::out_of_range& ex)
    {
        _packData[serial] = new ZendurePackStats(serial);
    }

    _packData[serial]->update(packData, ms);


    calculateAggregatedPackData();
}

void ZendureBatteryStats::ZendurePackStats::update(JsonObjectConst packData, unsigned int ms){
    _lastUpdateTimestamp = ms;

    auto power = Utils::getJsonElement<int16_t>(packData, "power");
    if (power.has_value()){
        _power = *power;
    }

    auto soc_level = Utils::getJsonElement<uint8_t>(packData, "socLevel");
    if (power.has_value()){
        _soc_level = *soc_level;
    }

    auto state = Utils::getJsonElement<uint8_t>(packData, "state");
    if (state.has_value()){
        _state = *state;
    }

    auto cell_temp_max = Utils::getJsonElement<float>(packData, "maxTemp");
    if (state.has_value()){
        *cell_temp_max -= 2731;
        *cell_temp_max /= 10;

        if (*cell_temp_max > -100 && *cell_temp_max < 100) {
            _cell_temperature_max = *cell_temp_max;
        }
    }

    auto total_voltage = Utils::getJsonElement<float>(packData, "totalVol");
    if (total_voltage.has_value()){
        *total_voltage /= 100;
        if (*total_voltage > 0 && *total_voltage < 65){
            _voltage_total = *total_voltage;
            _totalVoltageTimestamp = _lastUpdateTimestamp;
        }
    }

    auto cell_voltage_max = Utils::getJsonElement<uint16_t>(packData, "maxVol");
    if (cell_voltage_max.has_value()){
        *cell_voltage_max *= 10;
        if (*cell_voltage_max > 2000 && *cell_voltage_max < 4000){
            _cell_voltage_max = *cell_voltage_max;
        }
    }

    auto cell_voltage_min = Utils::getJsonElement<uint16_t>(packData, "minVol");
    if (cell_voltage_min.has_value()){
        *cell_voltage_min *= 10;
        if (*cell_voltage_min > 2000 && *cell_voltage_min < 4000){
            _cell_voltage_min = *cell_voltage_min;
        }
    }

    auto version = Utils::getJsonElement<uint32_t>(packData, "softVersion");
    if (version.has_value()){
        _version = *version;
    }

    _cell_voltage_spread = _cell_voltage_max - _cell_voltage_min;

    if (_voltage_total){
        _current = static_cast<float>(_power) / _voltage_total;
    }
}

void ZendureBatteryStats::getLiveViewData(JsonVariant& root) const {
    BatteryStats::getLiveViewData(root);


    auto bool2str = [](bool value) -> std::string {
        return value ? "enabled" : "disabled";
    };

    auto addRemainingTime = [](auto root, auto section, const char* name, uint16_t value) {
        if (value >= 59940){
            addLiveViewTextInSection(root, section, name, "unavail");
        }else{
            addLiveViewInSection(root, section, name, value, "min", 0);
        }
    };

    // values go into the "Status" card of the web application
    std::string section("status");
    addLiveViewInSection(root, section, "totalInputPower", _input_power, "W", 0);
    addLiveViewInSection(root, section, "chargePower", _charge_power, "W", 0);
    addLiveViewInSection(root, section, "dischargePower", _discharge_power, "W", 0);
    addLiveViewInSection(root, section, "totalOutputPower", _output_power, "W", 0);
    addLiveViewInSection(root, section, "efficiency", _efficiency, "%", 3);
    addLiveViewInSection(root, section, "capacity", _capacity, "Wh", 0);
    addLiveViewInSection(root, section, "availableCapacity", getAvailableCapacity(), "Wh", 0);
    addLiveViewTextInSection(root, section, "state", getStateString());
    addLiveViewTextInSection(root, section, "heatState", bool2str(_heat_state));
    addLiveViewTextInSection(root, section, "bypassState", bool2str(_bypass_state));
    addLiveViewInSection(root, section, "batteries", _num_batteries, "", 0);
    addRemainingTime(root, section, "remainOutTime", _remain_out_time);
    addRemainingTime(root, section, "remainInTime", _remain_in_time);

    // values go into the "Settings" card of the web application
    section = "settings";
    addLiveViewInSection(root, section, "maxInversePower", _inverse_max, "W", 0);
    addLiveViewInSection(root, section, "outputLimit", _output_limit, "W", 0);
    addLiveViewInSection(root, section, "inputLimit", _output_limit, "W", 0);
    addLiveViewInSection(root, section, "minSoC", _soc_min, "%", 1);
    addLiveViewInSection(root, section, "maxSoC", _soc_max, "%", 1);
    addLiveViewTextInSection(root, section, "autoRecover", bool2str(_auto_recover));
    addLiveViewTextInSection(root, section, "autoShutdown", bool2str(_auto_shutdown));
    addLiveViewTextInSection(root, section, "bypassMode", getBypassModeString());
    addLiveViewTextInSection(root, section, "buzzer", bool2str(_buzzer));

    // values go into the "Solar Panels" card of the web application
    section = "panels";
    addLiveViewInSection(root, section, "solarInputPower1", _solar_power_1, "W", 0);
    addLiveViewInSection(root, section, "solarInputPower2", _solar_power_2, "W", 0);

    // pack data goes to dedicated cards of the web application
    char buff[30];
    for (const auto& [sn, value] : _packData){
        snprintf(buff, sizeof(buff), "_%s [%s]", value->_name.c_str(), sn.c_str());
        section = std::string(buff);
        addLiveViewTextInSection(root, section, "state", value->getStateString());
        addLiveViewInSection(root, section, "cellMaxTemperature", value->_cell_temperature_max, "°C", 1);
        addLiveViewInSection(root, section, "cellMinVoltage", value->_cell_voltage_min, "mV", 0);
        addLiveViewInSection(root, section, "cellMaxVoltage", value->_cell_voltage_max, "mV", 0);
        addLiveViewInSection(root, section, "cellDiffVoltage", value->_cell_voltage_spread, "mV", 0);
        addLiveViewInSection(root, section, "voltage", value->_voltage_total, "V", 2);
        addLiveViewInSection(root, section, "power", value->_power, "W", 0);
        addLiveViewInSection(root, section, "current", value->_current, "A", 2);
        addLiveViewInSection(root, section, "SoC", value->_soc_level, "%", 1);
        addLiveViewInSection(root, section, "capacity", value->_capacity, "Wh", 0);
        addLiveViewInSection(root, section, "FwVersion", value->_version, "", 0);
    }

    // values go into the alarms card of the web application
    addLiveViewAlarm(root, "lowSOC", _alarmLowSoC);
    addLiveViewAlarm(root, "lowVoltage", _alarmLowVoltage);
    addLiveViewAlarm(root, "highVoltage", _alarmHightVoltage);
    addLiveViewAlarm(root, "underTemperatureCharge", _alarmLowTemperature);
    addLiveViewAlarm(root, "overTemperatureCharge", _alarmHighTemperature);
}

void ZendureBatteryStats::mqttPublish() const {
    BatteryStats::mqttPublish();

    MqttSettings.publish("battery/cellMinMilliVolt", String(_cellMinMilliVolt));
    MqttSettings.publish("battery/cellMaxMilliVolt", String(_cellMaxMilliVolt));
    MqttSettings.publish("battery/cellDiffMilliVolt", String(_cellDeltaMilliVolt));
    MqttSettings.publish("battery/cellMaxTemperature", String(_cellTemperature));
    MqttSettings.publish("battery/chargePower", String(_charge_power));
    MqttSettings.publish("battery/dischargePower", String(_discharge_power));
    MqttSettings.publish("battery/heating", String(static_cast<uint8_t>(_heat_state)));
    MqttSettings.publish("battery/state", String(_state));
    MqttSettings.publish("battery/numPacks", String(_num_batteries));

    for (const auto& [sn, value] : _packData){
        MqttSettings.publish("battery/" + sn + "/cellMinMilliVolt", String(value->_cell_voltage_min));
        MqttSettings.publish("battery/" + sn + "/cellMaxMilliVolt", String(value->_cell_voltage_max));
        MqttSettings.publish("battery/" + sn + "/cellDiffMilliVolt", String(value->_cell_voltage_spread));
        MqttSettings.publish("battery/" + sn + "/cellMaxTemperature", String(value->_cell_temperature_max));
        MqttSettings.publish("battery/" + sn + "/voltage", String(value->_voltage_total));
        MqttSettings.publish("battery/" + sn + "/power", String(value->_power));
        MqttSettings.publish("battery/" + sn + "/current", String(value->_current));
        MqttSettings.publish("battery/" + sn + "/stateOfCharge", String(value->_soc_level));
        MqttSettings.publish("battery/" + sn + "/state", String(value->_state));
    }

    MqttSettings.publish("battery/solarPowerMppt1", String(_solar_power_1));
    MqttSettings.publish("battery/solarPowerMppt2", String(_solar_power_2));
    MqttSettings.publish("battery/outputPower", String(_output_power));
    MqttSettings.publish("battery/inputPower", String(_input_power));
    MqttSettings.publish("battery/outputLimitPower", String(_output_limit));
   // MqttSettings.publish("battery/inputLimitPower", String(_output_limit));
    MqttSettings.publish("battery/bypass", String(static_cast<uint8_t>(_bypass_state)));
}

std::string ZendureBatteryStats::getBypassModeString() const {
    switch (_bypass_mode) {
        case 0:
            return "auto";
        case 1:
            return "alwaysoff";
        case 2:
            return "alwayson";
        default:
            return "invalid";
    }
}

std::string ZendureBatteryStats::getStateString(uint8_t state) {
    switch (state) {
        case 0:
            return "idle";
        case 1:
            return "charging";
        case 2:
            return "discharging";
        default:
            return "invalid";
    }
}

void ZendureBatteryStats::calculateAggregatedPackData() {
    // calcualte average voltage over all battery packs
    float voltage = 0.0;
    float temp = 0.0;
    uint32_t cellMin = UINT32_MAX;
    uint32_t cellMax = 0;
    uint16_t capacity = 0;

    uint32_t timestampVoltage = 0;

    size_t countVoltage = 0;
    size_t countValid = 0;


    bool alarmLowSoC = false;
    bool alarmTempLow = false;
    bool alarmTempHigh = false;
    bool alarmLowVoltage = false;
    bool alarmHighVoltage = false;

    for (const auto& [sn, value] : _packData){
        capacity += value->getCapacity();

        // sum all pack voltages
        if (value->_totalVoltageTimestamp){
            voltage += value->_voltage_total;
            timestampVoltage = max(timestampVoltage, value->_totalVoltageTimestamp);

            alarmLowVoltage |= value->hasAlarmLowVoltage();
            alarmHighVoltage |= value->hasAlarmHighVoltage();

            countVoltage++;
        }

        // aggregate remaining values
        if (value->_lastUpdateTimestamp){
            temp = max(temp, value->_cell_temperature_max);

            cellMax = max(cellMax, static_cast<uint32_t>(value->_cell_voltage_max));
            if (value->_cell_voltage_min){
                cellMin = min(cellMin, static_cast<uint32_t>(value->_cell_voltage_min));
            }

            alarmLowSoC |= value->hasAlarmLowSoC();
            alarmTempLow |= value->hasAlarmMinTemp();
            alarmTempHigh |= value->hasAlarmMaxTemp();

            countValid++;
        }
    }

    if (countVoltage){
        setVoltage(voltage / countVoltage, timestampVoltage);

        _alarmLowVoltage = alarmLowVoltage;
        _alarmHightVoltage = alarmHighVoltage;
    }

    if(countValid){
        _cellMaxMilliVolt = static_cast<uint16_t>(cellMax);
        _cellMinMilliVolt = static_cast<uint16_t>(cellMin);
        _cellDeltaMilliVolt = _cellMaxMilliVolt - _cellMinMilliVolt;

        _cellTemperature = temp;
        _alarmHighTemperature = alarmTempHigh;
        _alarmLowTemperature = alarmTempLow;
        _alarmLowSoC = alarmLowSoC;
    }

    _capacity = capacity;
}

void ZendureBatteryStats::calculateEfficiency() {
    float in = _input_power;
    float out = _output_power;
    if (isCharging()){
        out += _charge_power;
    }
    if (isDischarging()){
        in += _discharge_power;
    }

    _efficiency = in ? out / in : 0.0;
    _efficiency *= 100;
}


void ZendureBatteryStats::ZendurePackStats::setSerial(String serial){
    if (serial.startsWith("CO4H")){
        _capacity = 1920;
        _name = "AB2000";
    }
    _serial = serial;
}
#endif
