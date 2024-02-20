// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_PYLONTECH_RS485_RECEIVER

// #define PYLONTECH_RS485_DEBUG_ENABLED

#include "PylontechRS485Receiver.h"
#include "Battery.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PinMapping.h"
#include <Arduino.h>
#include <ctime>

static constexpr char TAG[] = "[Pylontech RS485]";

bool PylontechRS485Receiver::init()
{
    _lastBatteryCheck.set(Configuration.get().Battery.PollInterval * 1000);

    if (!Configuration.get().Battery.Enabled)
        return false;

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("Initialize Pylontech battery interface RS485 port rx = %d, tx = %d rts = %d. ", pin.battery_rx, pin.battery_tx, pin.battery_rts);

    if (pin.battery_rx < 0 || pin.battery_tx < 0 || pin.battery_rts < 0) {
        MessageOutput.println("Invalid pin config");
        return false;
    }

    if (!_stats->_initialized) {
        RS485.begin(115200, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
        RS485.setPins(pin.battery_rx, pin.battery_tx, UART_PIN_NO_CHANGE, pin.battery_rts);
        ESP_ERROR_CHECK(uart_set_mode(2, UART_MODE_RS485_HALF_DUPLEX));

        // Set read timeout of UART TOUT feature
        ESP_ERROR_CHECK(uart_set_rx_timeout(2, ECHO_READ_TOUT));

        while (RS485.available()) { // clear RS485 read buffer
            RS485.read();
            vTaskDelay(1);
        }

        MessageOutput.println("initialized successfully.");

        _stats->_initialized = true;
    }

    MessageOutput.print("Read basic infos and parameters. ");
//    boolean temp = Battery._verboseLogging;
//Battery._verboseLogging = true;
    get_protocol_version();
    get_manufacturer_info();
    get_module_serial_number();
    get_firmware_info();

    send_cmd(2, Command::GetSystemParameter, false);
    vTaskDelay(100);
    get_system_parameters();

    get_pack_count();
//    get_barcode();
//    get_version_info();

    send_cmd(2, Command::GetChargeDischargeManagementInfo);
    vTaskDelay(100);
    get_charge_discharge_management_info();

    send_cmd(2, Command::GetAnalogValue);
    vTaskDelay(100);
    get_analog_value();

    send_cmd(2, Command::GetAlarmInfo);
    vTaskDelay(100);
    get_alarm_info();
//    Battery._verboseLogging = temp;

    _isInstalled = true;

    MessageOutput.println("done");

    return true;
}

void PylontechRS485Receiver::deinit()
{
    if (!_isInstalled) {
        return;
    }

    MessageOutput.printf("%s RS485 driver uninstalled\r\n", TAG);
    _isInstalled = false;
}

void PylontechRS485Receiver::loop()
{
    Battery_CONFIG_T& cBattery = Configuration.get().Battery;

    if (!cBattery.Enabled || !_stats->_initialized) {
        if (!cBattery.Enabled)
            deinit();
        return;
    }

    if (!_isInstalled) {
        init();
        return;
    }
    if (RS485.available()) {
        uint32_t t_start = millis();
        switch (_lastCmnd) {
        case Command::GetChargeDischargeManagementInfo:
            get_charge_discharge_management_info();
            break;
        case Command::GetAlarmInfo:
            get_alarm_info();
            break;
        case Command::GetAnalogValue:
            get_analog_value();
            break;
        case Command::GetSystemParameter:
            get_system_parameters();
            break;
        case 0xff:
            while (RS485.available())
                Serial.printf("%02X ", RS485.read());
            break;
        }
        MessageOutput.printf("%s response time %lu ms, Cmnd: %02X\r\n", TAG, millis() - t_start, _lastCmnd);
        _lastCmnd = 0xFF;

        _stats->setLastUpdate(millis());
    }

    if (!_lastBatteryCheck.occured()) {
        return;
    }
    _lastBatteryCheck.set(Configuration.get().Battery.PollInterval * 1000);

    typedef struct {
        Command cmd;
        boolean Pack;
    } Commands_t;

    static const Commands_t Commands[] = {
        { Command::GetAnalogValue, true},
        { Command::GetChargeDischargeManagementInfo, true},
        { Command::GetAnalogValue, true},
        { Command::GetAlarmInfo, true},
        { Command::GetAnalogValue, true},
        { Command::GetSystemParameter, false},
    };

    static uint8_t state = 0;
    send_cmd(2, Commands[state].cmd, Commands[state].Pack);
    state++;
    if (state >= sizeof(Commands)/sizeof(Commands_t)) state=0;

    MessageOutput.printf("%s request time %u ms, Cmnd: %02X\r\n", TAG, _lastBatteryCheck.elapsed(), _lastCmnd);
}

void PylontechRS485Receiver::get_protocol_version(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetProtocolVersion, false);
    yield();
    if (read_frame() == NULL) {
        MessageOutput.printf("%s Battery not responding, check cabling or battery power switch\r\f", TAG);
    };
    yield();

    return;
}

void PylontechRS485Receiver::get_barcode(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetBarCode);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;
}

void PylontechRS485Receiver::get_firmware_info(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetFirmwareInfo);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    uint8_t* info = &f->info[1];

    uint32_t version = ToUint16(info);
    info+=2;
    _stats->_manufacturerVersion = String((version >> 8) & 0x0F) + '.' + String(version & 0x0F);
    version = ToUint24(info);
    _stats->_mainLineVersion = String((version >> 16) & 0x0F) + '.' + String((version >> 8) & 0x0F) + '.' + String(version & 0x0F);

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s Manufacturer Version: '%s'\r\n", TAG, _stats->_manufacturerVersion.c_str());
        MessageOutput.printf("%s Main Line Version: '%s'\r\n", TAG, _stats->_mainLineVersion.c_str());
    }
}

void PylontechRS485Receiver::get_version_info(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetVersionInfo);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;
}

void PylontechRS485Receiver::get_pack_count(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetPackCount);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    _number_of_packs = f->info[0];

    if (Battery._verboseLogging)
        MessageOutput.printf("%s::%s Number of Battery Packs %u\r\n", TAG, __FUNCTION__, _number_of_packs);
}

void PylontechRS485Receiver::get_manufacturer_info(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetManufacturerInfo, false);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    uint8_t* info = &f->info[0];

    String deviceName(reinterpret_cast<char*>(info), 10);
    uint16_t sw_version = ToUint16(info + 10);
    _stats->softwareVersion = String((sw_version >> 8) & 0x0F) + '.' + String(sw_version & 0x0F);
    String manufacturer(reinterpret_cast<char*>(info + 12), 20);

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s Manufacturer: '%s' size: %d\r\n", TAG, manufacturer.c_str(), manufacturer.length());
        MessageOutput.printf("%s Software Version: %s\r\n", TAG, _stats->softwareVersion.c_str());
        MessageOutput.printf("%s Device Name: '%s' size: %d\r\n", TAG, deviceName.c_str(), deviceName.length());
    }

    while (manufacturer.length() > 0) {
        int lastIndex = manufacturer.length() - 1;
        if (isblank(manufacturer.charAt(lastIndex)) || manufacturer.charAt(lastIndex) == '-' || manufacturer.charAt(lastIndex) == 0)
            manufacturer.remove(lastIndex);
        else
            break;
    }

    while (deviceName.length() > 0) {
        int lastIndex = deviceName.length() - 1;
        if (isblank(deviceName.charAt(lastIndex)) || deviceName.charAt(lastIndex) == '-' || deviceName.charAt(lastIndex) == 0)
            deviceName.remove(lastIndex);
        else
            break;
    }

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s Manufacturer: '%s' size: %d\r\n", TAG, manufacturer.c_str(), manufacturer.length());
        MessageOutput.printf("%s Device Name: '%s' size: %d\r\n", TAG, deviceName.c_str(), deviceName.length());
    }

    _stats->setManufacturer(std::move(manufacturer));
    _stats->setDeviceName(std::move(deviceName));

    if (Battery._verboseLogging)
        MessageOutput.printf("%s Device Name: '%s' Manufacturer Name: '%s'\r\n", TAG,
            _stats->getDeviceName().c_str(), _stats->getManufacturer().c_str());
}

void PylontechRS485Receiver::get_analog_value()
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = INFOFLAG + DATAI
    // uint8_t InfoFlag = f->info[0];
    uint8_t* info = &(f->info[1]);

    _stats->numberOfModule = *info++;
    _stats->numberOfCells = *info++;

    if (Battery._verboseLogging)
        MessageOutput.printf("%s Number of Module: %d, Number of Cells: %d\r\n", TAG, _stats->numberOfModule, _stats->numberOfCells);

    _stats->cellMinVoltage = 999999.0;
    _stats->cellMaxVoltage = -999999.0;
    _stats->CellVoltages = reinterpret_cast<float*>(realloc(_stats->CellVoltages, _stats->numberOfCells * sizeof(float)));
    for (int i = 0; i < _stats->numberOfCells; i++) {
        float value = to_CellVolt(info);
        info += 2;
        _stats->CellVoltages[i] = value;
        if (_stats->cellMinVoltage > value)
            _stats->cellMinVoltage = value;
        if (_stats->cellMaxVoltage < value)
            _stats->cellMaxVoltage = value;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        MessageOutput.printf("%s CellVoltage[%d]: %.3f\r\n", TAG, i + 1, _stats->CellVoltages[i]);
#endif
    }
    _stats->cellDiffVoltage = (_stats->cellMaxVoltage - _stats->cellMinVoltage) * 1000.0; // in mV

    _stats->numberOfTemperatures = *info++;
    if (Battery._verboseLogging)
        MessageOutput.printf("%s Number of Temperatures: %d\r\n", TAG, _stats->numberOfTemperatures);

    _stats->averageBMSTemperature = to_Celsius(info);
    info += 2;
    _stats->averageCellTemperature = 0.0;
    _stats->minCellTemperature = 10000.0;
    _stats->maxCellTemperature = -10000.0;
    _stats->GroupedCellsTemperatures = reinterpret_cast<float*>(realloc(_stats->GroupedCellsTemperatures, (_stats->numberOfTemperatures - 1) * sizeof(float)));
    for (int i = 0; i < _stats->numberOfTemperatures - 1; i++) {
        float t = to_Celsius(info);
        info += 2;
        _stats->GroupedCellsTemperatures[i] = t;
        _stats->averageCellTemperature += t;
        if (_stats->minCellTemperature > t)
            _stats->minCellTemperature = t;
        if (_stats->maxCellTemperature < t)
            _stats->maxCellTemperature = t;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        MessageOutput.printf("%s GroupedCellsTemperatures[%d]: %.1f\r\n", TAG, i + 1, _stats->GroupedCellsTemperatures[i]);
#endif
    }
    _stats->averageCellTemperature /= (_stats->numberOfTemperatures - 1);
    _stats->current = to_Amp(info);
    info += 2;
    _stats->voltage = to_Volt(info);
    info += 2;
    _stats->chargeVoltage = _stats->voltage;
    _stats->power = _stats->current * _stats->voltage / 1000.0; // kW
    _stats->remainingCapacity = DivideUint16By1000(info);
    info += 2; // DivideBy1000(Int16ub),
    uint8_t _UserDefinedItems = *info++;
    _stats->totalCapacity = DivideUint16By1000(info);
    info += 2;
    _stats->cycles = ToUint16(info);
    info += 2;
    if (_UserDefinedItems > 2) {
        _stats->remainingCapacity = DivideUint24By1000(info);
        info += 3;
        _stats->totalCapacity = DivideUint24By1000(info);
        info += 3;
    }
    _stats->totalPower = _stats->power;
    _stats->setSoC(static_cast<uint8_t>((100.0 * _stats->remainingCapacity / _stats->totalCapacity) + 0.5));
    if (Battery._verboseLogging) {
        MessageOutput.printf("%s AverageBMSTemperature: %.1f\r\n", TAG, _stats->averageBMSTemperature);
        MessageOutput.printf("%s AverageCellTemperature: %.1f\r\n", TAG, _stats->averageCellTemperature);
        MessageOutput.printf("%s MinCellTemperature: %.1f\r\n", TAG, _stats->minCellTemperature);
        MessageOutput.printf("%s MaxCellTemperature: %.1f\r\n", TAG, _stats->maxCellTemperature);
        MessageOutput.printf("%s Current: %.1f, Voltage: %.3f\r\n", TAG, _stats->current, _stats->voltage);
        MessageOutput.printf("%s Capacity Remaining: %.3f, Total: %.3f\r\n", TAG, _stats->remainingCapacity, _stats->totalCapacity);
        MessageOutput.printf("%s Total Power: %.3f kW\r\n", TAG, _stats->totalPower);
        MessageOutput.printf("%s State of Charge: %d%%, Cycles: %d\r\n", TAG, _stats->getSoC(), _stats->cycles);
    }
}

void PylontechRS485Receiver::get_analog_value_multiple(void)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(2, Command::GetAnalogValue, true);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = INFOFLAG + DATAI
    // uint8_t InfoFlag = f->info[0];
    // uint8_t CommandValue = f->info[1];
    //    uint8_t *info = &(f->info[2]);
}

void PylontechRS485Receiver::get_system_parameters()
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = INFOFLAG + DATAI
    uint8_t* info = &(f->info[1]);
    _stats->SystemParameters.cellHighVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.cellLowVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.cellUnderVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.chargeHighTemperatureLimit = to_Celsius(info);
    info += 2;
    _stats->SystemParameters.chargeLowTemperatureLimit = to_Celsius(info);
    info += 2;
    _stats->SystemParameters.chargeCurrentLimit = to_Amp(info);
    info += 2;
    _stats->SystemParameters.moduleHighVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.moduleLowVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.moduleUnderVoltageLimit = to_Volt(info);
    info += 2;
    _stats->SystemParameters.dischargeHighTemperatureLimit = to_Celsius(info);
    info += 2;
    _stats->SystemParameters.dischargeLowTemperatureLimit = to_Celsius(info);
    info += 2;
    _stats->SystemParameters.dischargeCurrentLimit = to_Amp(info);
    info += 2;

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s CurrentLimit charge: %.2fA, discharge: %.2fA\r\n", TAG,
            _stats->SystemParameters.chargeCurrentLimit,
            _stats->SystemParameters.dischargeCurrentLimit);
        MessageOutput.printf("%s Cell VoltageLimit High: %.3fV, Low: %.3fV, Under: %.3fV\r\n", TAG,
            _stats->SystemParameters.cellHighVoltageLimit,
            _stats->SystemParameters.cellLowVoltageLimit,
            _stats->SystemParameters.cellUnderVoltageLimit);
        MessageOutput.printf("%s Charge TemperatureLimit High: %.1f°C, Low: %.1f°C\r\n", TAG,
            _stats->SystemParameters.chargeHighTemperatureLimit,
            _stats->SystemParameters.chargeLowTemperatureLimit);
        MessageOutput.printf("%s Module VoltageLimit High: %.3fV, Low: %.3fV, Under: %.3fV\r\n", TAG,
            _stats->SystemParameters.moduleHighVoltageLimit,
            _stats->SystemParameters.moduleLowVoltageLimit,
            _stats->SystemParameters.moduleUnderVoltageLimit);
        MessageOutput.printf("%s Discharge TemperatureLimit High: %.1f°C, Low: %.1f°C\r\n", TAG,
            _stats->SystemParameters.dischargeHighTemperatureLimit,
            _stats->SystemParameters.dischargeLowTemperatureLimit);
    }
}

void PylontechRS485Receiver::get_alarm_info()
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = DATAFLAG + WARNSTATE
    // uint8_t DataFlag = f->info[0];
    // uint8_t CommandValue = f->info[1];
    uint8_t* info = &f->info[2];

    _stats->AlarmInfo.numberOfCells = *info++;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    char buffer[128];
#endif
    _stats->Warning.lowVoltage = 0;
    _stats->Warning.highVoltage = 0;
    _stats->AlarmInfo.cellVoltages = reinterpret_cast<uint8_t*>(realloc(_stats->AlarmInfo.cellVoltages, _stats->AlarmInfo.numberOfCells * sizeof(uint8_t)));
    for (int i = 0; i < _stats->AlarmInfo.numberOfCells; i++) {
        _stats->AlarmInfo.cellVoltages[i] = *info++;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        snprintf(&buffer[i * 3], 4, "%02X ", _stats->AlarmInfo.CellVoltages[i]);
#endif
        if (_stats->AlarmInfo.cellVoltages[i] & 0x01)
            _stats->Warning.lowVoltage = 1;
        if (_stats->AlarmInfo.cellVoltages[i] & 0x02)
            _stats->Warning.highVoltage = 1;
    }
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s Number of Cell: %d, Status: %s\r\n", TAG, _stats->AlarmInfo.NumberOfCells, buffer);
#endif

    _stats->AlarmInfo.numberOfTemperatures = *info++;
    _stats->Warning.lowTemperature = 0; // set to normal
    _stats->Warning.highTemperature = 0; // set nor normal

    _stats->AlarmInfo.BMSTemperature = *info++;
    if (_stats->AlarmInfo.BMSTemperature & 0x01)
        _stats->Warning.lowTemperature = 1;
    if (_stats->AlarmInfo.BMSTemperature & 0x02)
        _stats->Warning.highTemperature = 1;

    _stats->AlarmInfo.Temperatures = reinterpret_cast<uint8_t*>(realloc(_stats->AlarmInfo.Temperatures, (_stats->AlarmInfo.numberOfTemperatures - 1) * sizeof(uint8_t)));
    for (int i = 0; i < _stats->numberOfTemperatures - 1; i++) {
        _stats->AlarmInfo.Temperatures[i] = *info++;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        snprintf(&buffer[i * 3], 4, "%02X ", _stats->AlarmInfo.Temperatures[i]);
#endif
        if (_stats->AlarmInfo.Temperatures[i] & 0x01)
            _stats->Warning.lowTemperature = 1;
        if (_stats->AlarmInfo.Temperatures[i] & 0x02)
            _stats->Warning.highTemperature = 1;
    }
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s Number of Temperature Sensors: %d,  Status: %s\r\n", TAG, _stats->AlarmInfo.NumberOfTemperatures, buffer);
#endif

    _stats->AlarmInfo.chargeCurrent = *info++;
    _stats->Warning.highCurrentCharge = (_stats->AlarmInfo.chargeCurrent & 0x02) ? 1 : 0;

    _stats->AlarmInfo.moduleVoltage = *info++;
    if (_stats->AlarmInfo.moduleVoltage & 0x01)
        _stats->Warning.lowVoltage = 1;
    if (_stats->AlarmInfo.moduleVoltage & 0x02)
        _stats->Warning.highVoltage = 1;

    _stats->AlarmInfo.dischargeCurrent = *info++;
    _stats->Warning.highCurrentDischarge = (_stats->AlarmInfo.dischargeCurrent & 0x02) ? 1 : 0;

    _stats->AlarmInfo.status1 = *info++;
    _stats->Alarm.underVoltage = _stats->AlarmInfo.moduleUnderVoltage;
    _stats->Alarm.overVoltage = _stats->AlarmInfo.moduleOverVoltage;
    _stats->Alarm.overCurrentCharge = _stats->AlarmInfo.chargeOverCurrent;
    _stats->Alarm.overCurrentDischarge = _stats->AlarmInfo.dischargeOverCurrent;
    _stats->Alarm.underTemperature = _stats->averageBMSTemperature < _stats->SystemParameters.dischargeLowTemperatureLimit;
    _stats->Alarm.overTemperature = _stats->AlarmInfo.dischargeOverTemperature | _stats->AlarmInfo.chargeOverTemperature;

    _stats->AlarmInfo.status2 = *info++;
    _stats->AlarmInfo.status3 = *info++;
    _stats->AlarmInfo.status4 = *info++;
    _stats->AlarmInfo.status5 = *info;
    for (int i = 0; i < _stats->AlarmInfo.numberOfCells; i++) {
        if ((_stats->AlarmInfo.cellVoltages[i] & 0x01) && (_stats->AlarmInfo.cellError & (1 << i)))
            _stats->Alarm.underVoltage = 1;
        if ((_stats->AlarmInfo.cellVoltages[i] & 0x02) && (_stats->AlarmInfo.cellError & (1 << i)))
            _stats->Alarm.overVoltage = 1;
    }

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s Status ChargeCurrent: %02X ModuleVoltage: %02X DischargeCurrent: %02X\r\n", TAG,
            _stats->AlarmInfo.chargeCurrent, _stats->AlarmInfo.moduleVoltage, _stats->AlarmInfo.dischargeCurrent);
        MessageOutput.printf("%s Status 1: MOV:%s, CUV:%s, COC:%s, DOC:%s, DOT:%s, COT:%s, MUV:%s\r\n", TAG,
            _stats->AlarmInfo.moduleOverVoltage ? "trigger" : "normal",
            _stats->AlarmInfo.cellUnderVoltage ? "trigger" : "normal",
            _stats->AlarmInfo.chargeOverCurrent ? "trigger" : "normal",
            _stats->AlarmInfo.dischargeOverCurrent ? "trigger" : "normal",
            _stats->AlarmInfo.dischargeOverTemperature ? "trigger" : "normal",
            _stats->AlarmInfo.chargeOverTemperature ? "trigger" : "normal",
            _stats->AlarmInfo.moduleUnderVoltage ? "trigger" : "normal");
        MessageOutput.printf("%s Status 2: %s using battery module power, Discharge MOSFET %s, Charge MOSFEET %s, Pre MOSFET %s\r\n", TAG,
            _stats->AlarmInfo.usingBatteryModulePower ? "" : "not",
            _stats->AlarmInfo.dischargeMOSFET ? "ON" : "OFF",
            _stats->AlarmInfo.chargeMOSFEET ? "ON" : "OFF",
            _stats->AlarmInfo.preMOSFET ? "ON" : "OFF");
        MessageOutput.printf("%s Status 3: buzzer %s, fully Charged (SOC=%s), discharge current detected by BMS %s, charge current detected by BMS %s\r\n", TAG,
            _stats->AlarmInfo.buzzer ? "on" : "off",
            _stats->AlarmInfo.fullyCharged ? "100%" : "normal",
            _stats->AlarmInfo.effectiveDischargeCurrent ? "＞0.1A" : "normal",
            _stats->AlarmInfo.effectiveChargeCurrent ? "<-0.1A" : "normal");
        MessageOutput.printf("%s Cell Error: %04X\r\n", TAG, _stats->AlarmInfo.cellError);
    }
    /*
        _stats->alarm.BmsInternal =
    */
}

void PylontechRS485Receiver::get_charge_discharge_management_info()
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = DATAI
    // uint8_t CommandValue = f->info[0];
    uint8_t* info = &(f->info[1]);

    _stats->ChargeDischargeManagementInfo.chargeVoltageLimit = to_Volt(info);
    info += 2;
    _stats->ChargeDischargeManagementInfo.dischargeVoltageLimit = to_Volt(info);
    info += 2;
    _stats->ChargeDischargeManagementInfo.chargeCurrentLimit = to_Amp(info);
    info += 2;
    _stats->ChargeDischargeManagementInfo.dischargeCurrentLimit = to_Amp(info);
    info += 2;
    _stats->ChargeDischargeManagementInfo.Status = *info;

    if (Battery._verboseLogging) {
        MessageOutput.printf("%s CVL:%.3fV, DVL:%.3fV, CCL:%.1fA, DCL:%.1fA, CE:%d DE:%d CI:%d CI1:%d FCR:%d\r\n", TAG,
            _stats->ChargeDischargeManagementInfo.chargeVoltageLimit,
            _stats->ChargeDischargeManagementInfo.dischargeVoltageLimit,
            _stats->ChargeDischargeManagementInfo.chargeCurrentLimit,
            _stats->ChargeDischargeManagementInfo.dischargeCurrentLimit,
            _stats->ChargeDischargeManagementInfo.chargeEnable,
            _stats->ChargeDischargeManagementInfo.dischargeEnable,
            _stats->ChargeDischargeManagementInfo.chargeImmediately,
            _stats->ChargeDischargeManagementInfo.chargeImmediately1,
            _stats->ChargeDischargeManagementInfo.fullChargeRequest);
    }
}

void PylontechRS485Receiver::get_module_serial_number(uint8_t devid)
{
    if (Battery._verboseLogging) MessageOutput.printf("%s::%s\r\n", TAG, __FUNCTION__);

    send_cmd(devid, Command::GetSerialNumber);
    yield();

    format_t* f = read_frame();
    yield();

    if (f == NULL)
        return;

    // INFO = DATAI
    // uint8_t CommandValue = f->info[0];
    uint8_t* info = &(f->info[1]);
    for (int i = 0; i < 16; i++)
        _stats->ModuleSerialNumber.moduleSerialNumber[i] = *info++;
    _stats->ModuleSerialNumber.moduleSerialNumber[16] = 0;

    if (Battery._verboseLogging)
        MessageOutput.printf("%s S/N %s\r\n", TAG, _stats->ModuleSerialNumber.moduleSerialNumber);
}

void PylontechRS485Receiver::setting_charge_discharge_management_info(uint8_t devid, const char* info)
{
    send_cmd(devid, Command::SetChargeDischargeManagementInfo);
    yield();
}

void PylontechRS485Receiver::turn_off_module(uint8_t devid)
{
    send_cmd(devid, Command::TurnOffModule);
    yield();

    /*format_t *f =*/read_frame();
}

uint16_t PylontechRS485Receiver::get_frame_checksum(char* frame)
{
    uint16_t sum = 0;

    while (*frame)
        sum += *frame++;
    sum = ~sum;
    sum %= 0x10000;
    sum += 1;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s checksum=%04X\r\n", TAG, sum);
#endif
    return sum;
}

uint16_t PylontechRS485Receiver::get_info_length(const char* info)
{
    uint8_t lenid = strlen(info);
    if (lenid == 0)
        return 0;

    uint8_t lenid_sum = (lenid & 0xf) + ((lenid >> 4) & 0xf) + ((lenid >> 8) & 0xf);
    uint8_t lenid_modulo = lenid_sum % 16;
    uint8_t lenid_invert_plus_one = 0b1111 - lenid_modulo + 1;

    uint16_t length = (lenid_invert_plus_one << 12) + lenid;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s info length: %04X\r\n", TAG, length);
#endif

    return length;
}

void PylontechRS485Receiver::_encode_cmd(char *frame, uint8_t address, uint8_t cid2, const char* info)
{
    char subframe[26];
    const uint8_t cid1 = 0x46;

    snprintf(subframe, sizeof(subframe), "%02X%02X%02X%02X%04X%s", 0x20, address, cid1, cid2, get_info_length(info), info);
    snprintf(frame, 32, "%c%s%04X%c", 0x7E, subframe, get_frame_checksum(subframe), 0x0D);
}

void PylontechRS485Receiver::send_cmd(uint8_t address, uint8_t cmd, boolean Pack)
{
    char raw_frame[32];
    char bdevid[3] = "";
    if (Pack==true) snprintf(bdevid, sizeof(bdevid), "%02X", address);
    _encode_cmd(raw_frame, address, cmd, bdevid);
    size_t length = strlen(raw_frame);

    if (Battery._verboseLogging) MessageOutput.printf("%s:%s frame=%s\r\n", TAG, __FUNCTION__, raw_frame);

    if (RS485.write(raw_frame, length) != length) {
        MessageOutput.printf("%s Send data critical failure.\r\n", TAG);
        // add your code to handle sending failure here
        //        abort();
    }
    _lastCmnd = cmd;
}

size_t PylontechRS485Receiver::readline(void)
{
    uint32_t t_end = millis() + 2000; // timeout 2 seconds
    while (millis() < t_end) {
        if (RS485.available())
            break;
        yield();
    }
    if (!RS485.available()) {
        // #ifdef PYLONTECH_RS485_DEBUG_ENABLED
        MessageOutput.printf("%s RS485 read timeout\r\n", TAG);
        // #endif
        return 0;
    }

    int idx = 0;
    bool start = false;

    t_end = millis() + 1000; // timeout 1 second
    while (millis() < t_end) {
        char c;
        if (RS485.available()) {
            c = RS485.read();
            if (!start && c != 0x7E) { // start of line ?
                yield();
                continue;
            }

            start = true;

            _received_frame[idx++] = c;

            if (c == 0x0D) { // end of line ?
                _received_frame[idx] = 0;
                break;
            }

            if (idx > 511) {
                MessageOutput.printf("%s RS485 buffer overflow\r\n", TAG);
                break;
            }
            yield();
        }
    }

    return idx;
}

uint32_t PylontechRS485Receiver::hex2binary(char* s, int len)
{
    uint32_t b = 0;

    if (len == 0)
        return 0;

    do {
        if (*s >= '0' && *s <= '9')
            b |= *s - '0';
        else if (*s >= 'A' && *s <= 'F')
            b |= *s - 'A' + 10;
        else if (*s >= 'a' && *s <= 'f')
            b |= *s - 'a' + 10;
        if (len > 1)
            b <<= 4;
        s++;
    } while (--len);
    return b;
}

char* PylontechRS485Receiver::_decode_hw_frame(size_t length)
{
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s _received_frame '%s' length=%d\r\n", TAG, _received_frame, length);
#endif
    if (length < 12)
        return NULL;

    char *frame_data = &_received_frame[1]; // remove SOI 0x7E
    _received_frame[length - 1] = 0; // remove EOI 0x0D
    length-=2;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s frame_data '%s' length=%d\r\n", TAG, frame_data, length);
#endif

#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    uint16_t frame_chksum;
    frame_chksum = strtol(&frame_data[length - 4], NULL, 16);
    MessageOutput.printf("%s send frame_chksum=%04X\r\n", TAG, frame_chksum);
#endif

    frame_data[length - 4] = 0; // remove frame checksum
    length-=4;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s frame_data '%s' length=%d\r\n", TAG, frame_data, length);
#endif
    get_frame_checksum(frame_data);
    //    assert(get_frame_checksum(frame_data) == frame_chksum);

    return frame_data;
}

format_t* PylontechRS485Receiver::_decode_frame(char* frame)
{
    format_t* f = reinterpret_cast<format_t*>(frame);
    size_t length = strlen(frame);
    f->ver = (uint8_t)hex2binary(&frame[0], 2);
    f->adr = (uint8_t)hex2binary(&frame[2], 2);
    f->cid1 = (uint8_t)hex2binary(&frame[4], 2);
    f->cid2 = (uint8_t)hex2binary(&frame[6], 2);
    f->infolength = (uint16_t)hex2binary(&frame[8], 4);

    if (Battery._verboseLogging)
        MessageOutput.printf("%s ver: %02X, adr: %02X, cid1: %02X, cid2: %02X, infolength: %d info:\r\n", TAG, f->ver, f->adr, f->cid1, f->cid2, (f->infolength) & 0xFFF);

    // f->info = (uint8_t*)realloc(f.info, f.infolength & 0xfff);
    for (int idx = 12, pos = 0; idx < length; idx += 2, pos++) {
        f->info[pos] = (uint8_t)hex2binary(&frame[idx], 2);
        if (Battery._verboseLogging) {
            MessageOutput.printf(" %02X", f->info[pos]);
            if (((pos + 1) % 16) == 0)
                MessageOutput.println();
        }
    }
    if (Battery._verboseLogging)
        MessageOutput.println();

    return f;
}

format_t* PylontechRS485Receiver::read_frame(void)
{
    size_t length;

    if (!(length=readline()))
        return NULL; // nothing read, timeout

    char* frame = _decode_hw_frame(length);
    if (frame == NULL)
        return NULL;

    format_t* parsed = _decode_frame(frame);
    return parsed;
}

#endif
