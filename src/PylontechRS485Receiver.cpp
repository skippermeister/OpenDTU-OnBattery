// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_PYLONTECH_RS485_RECEIVER

// #define PYLONTECH_RS485_DEBUG_ENABLED

#include "PylontechRS485Receiver.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PinMapping.h"
#include "SerialPortManager.h"
#include <driver/uart.h>
#include "defaults.h"
#include <Arduino.h>
#include <ctime>

static constexpr char TAG[] = "[Pylontech RS485]";

bool PylontechRS485Receiver::init()
{
    MessageOutput.printf("Initialize Pylontech battery interface... ");

    _lastBatteryCheck.set(Configuration.get().Battery.PollInterval * 1000);

/*
    if (!Configuration.get().Battery.Enabled) {
        MessageOutput.println("not enabled");
        return false;
    }
*/

    const PinMapping_t& pin = PinMapping.get();

    if (pin.battery_rx < 0 || pin.battery_tx < 0
        || pin.battery_rx == pin.battery_tx
        || (pin.battery_rts >= 0
            && (pin.battery_rts == pin.battery_rx || pin.battery_rts == pin.battery_tx))) {
        MessageOutput.println("Invalid pin config");
        return false;
    }

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    _upSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);

    _upSerial->begin(115200, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    MessageOutput.printf("Port= %d, RS485 (Type %d) port rx = %d, tx = %d", *oHwSerialPort, pin.battery_rts >= 0 ? 1 : 2, pin.battery_rx, pin.battery_tx);
    if (pin.battery_rts >= 0) {
        /*
         * Pylontech is connected via a RS485 module. Two different types of modules are supported.
         * Type 1: if a GPIO pin greater or equal 0 is given, we have a MAX3485 or SP3485 modul with external driven DE/RE pins
         *         Both pins are connected together and will be driven by the HWSerial driver.
         * Type 2: if the GPIO is negativ (-1), we assume that we have a RS485 TTL Modul with a self controlled DE/RE circuit.
         *         In this case we only need a TX and RX pin.
         */
        MessageOutput.printf(", rts = %d", pin.battery_rts);
        _upSerial->setPins(pin.battery_rx, pin.battery_tx, UART_PIN_NO_CHANGE, pin.battery_rts);
    }
    ESP_ERROR_CHECK(uart_set_mode(*oHwSerialPort, UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(*oHwSerialPort, ECHO_READ_TOUT));

    while (_upSerial->available()) { // clear RS485 read buffer
        _upSerial->read();
        vTaskDelay(1);
    }

    _received_frame = reinterpret_cast<char*>(malloc(128)); // receiver buffer with initially 128 bytes, will be expanded on request up to 256

    MessageOutput.println(", initialized successfully.");

    return true;
}

void PylontechRS485Receiver::readParameter()
{
    MessageOutput.printf("%s::%s: Read basic infos and parameters.\r\n", TAG, __FUNCTION__);
    bool temp = _verboseLogging;

    _verboseLogging = true;

    _masterBatteryID = 2; // Master battery starts with ID = 2

    // search the amount of connected batteries
    uint8_t number_of_packs = 0;
    for (uint8_t i = _masterBatteryID; i < _masterBatteryID + 8; i++) {
        if (get_pack_count(REQUEST_AND_GET, i) == false) {
            break;
        }
        number_of_packs++;
        vTaskDelay(500);
    }
    MessageOutput.printf("%s Found %d Battery Packs\r\n", TAG, number_of_packs);

    _stats->_number_of_packs = Configuration.get().Battery.numberOfBatteries;

    _lastSlaveBatteryID = _masterBatteryID + _stats->_number_of_packs;

    for (uint8_t i = _masterBatteryID; i < _lastSlaveBatteryID; i++) {
        MessageOutput.printf("%s %s Battery Pack %d\r\n", TAG, i == 2 ? "Master" : "Slave", i);
        get_protocol_version(REQUEST_AND_GET, i);
        vTaskDelay(500);
        get_manufacturer_info(REQUEST_AND_GET, i);
        vTaskDelay(500);
        get_module_serial_number(REQUEST_AND_GET, i);
        vTaskDelay(500);
        get_firmware_info(REQUEST_AND_GET, i);
        vTaskDelay(500);

        get_system_parameters(REQUEST_AND_GET, i);
        vTaskDelay(500);

        //        get_barcode(REQUEST_AND_GET, i);
        //          vTaskDelay(500);
        //        get_version_info(REQUEST_AND_GET, i);
        //          vTaskDelay(500);

        get_charge_discharge_management_info(REQUEST_AND_GET, i);
        vTaskDelay(500);

        get_analog_value(REQUEST_AND_GET, i);
        vTaskDelay(500);

        get_alarm_info(REQUEST_AND_GET, i);
        vTaskDelay(500);
    }
    _lastCmnd = 0xFF;

    _verboseLogging = temp;

    MessageOutput.printf("%s::%s done\r\n", TAG, __FUNCTION__);

    _initialized = true;
}

void PylontechRS485Receiver::deinit()
{
    _upSerial->end();

    if (PinMapping.get().battery_rts >= 0) { pinMode(PinMapping.get().battery_rts, INPUT); }

    SerialPortManager.freePort(_serialPortOwner);

    MessageOutput.printf("%s RS485 driver uninstalled\r\n", TAG);

    _initialized = false;
}

void PylontechRS485Receiver::loop()
{
    if (!_initialized) readParameter();

    static uint8_t BatteryID = _lastSlaveBatteryID;
    uint8_t module = BatteryID;
    if (module >= _lastSlaveBatteryID)
        module = _masterBatteryID; // inital check

    if (_upSerial->available()) {
        uint32_t t_start = millis();
        switch (_lastCmnd) {
        case Command::GetChargeDischargeManagementInfo:
            get_charge_discharge_management_info(PylontechRS485Receiver::Function::GET, module);
            break;
        case Command::GetAlarmInfo:
            get_alarm_info(PylontechRS485Receiver::Function::GET, module);
            break;
        case Command::GetAnalogValue:
            get_analog_value(PylontechRS485Receiver::Function::GET, module);
            break;
        case Command::GetSystemParameter:
            get_system_parameters(PylontechRS485Receiver::Function::GET, module);
            break;
        case 0xff:
            // Filtering out interference on the receiving line.
            // This shouldn't happen in normal operation, but it seems like some RS485 adapters/chips are very sensitive or out of specification.
            // In order not to block the scheduler too much, the filter will periodically read the input buffer for a maximum of 50ms per loop
            static uint16_t spikes = 0;
            while (_upSerial->available() && (millis() - t_start) < 50) {
                if (_upSerial->peek() != 0x7E) {
                    // we consume all incomming bytes which are not 0x7E (start sequence of a frame)
                    spikes++;
                    _upSerial->read();
                } else {
                    // this should never happen.
                    // It would mean that we had issued a command to the battery that was not recognized and processed in the parser above.
                    bool temp = _verboseLogging;
                    _verboseLogging = true; // switch on verbose logging, to see what happen
                    read_frame();
                    _verboseLogging = temp; // switch verbose logging to previous setting.
                    break;
                }
            }
            //
            if (spikes > 100) {
                MessageOutput.printf("%s Warning found %d signal interferences\r\n", TAG, spikes);
                spikes = 0;
            }
        }
        if (_lastCmnd != 0xFF) {
            MessageOutput.printf("%s response time %lu ms, Cmnd: %02X\r\n", TAG, millis() - t_start, _lastCmnd);

            // indicate that we have updated some battery values
            _stats->setLastUpdate(millis());
        }
        _lastCmnd = 0xFF;
    }

    if (!_lastBatteryCheck.occured()) { return; }
    _lastBatteryCheck.set(Configuration.get().Battery.PollInterval * 1000);

    typedef struct {
        Command cmd;
        uint8_t InfoCommand;
    } Commands_t;

    static const Commands_t Commands[] = {
        { Command::GetAnalogValue, 1 },
        { Command::GetChargeDischargeManagementInfo, 1 },
        { Command::GetAnalogValue, 1 },
        { Command::GetAlarmInfo, 1 },
        { Command::GetAnalogValue, 1 },
        { Command::GetSystemParameter, 0 },
    };

    static uint8_t state = 0;
    if (state == 0) {
        if (++BatteryID >= _lastSlaveBatteryID) BatteryID = _masterBatteryID;
    }
    MessageOutput.printf("%s %s Battery Pack %d\r\n", TAG, BatteryID == _masterBatteryID ? "Master" : "Slave", BatteryID);

    send_cmd(BatteryID, Commands[state].cmd, Commands[state].InfoCommand);
    if (++state >= sizeof(Commands) / sizeof(Commands_t)) state = 0;

    MessageOutput.printf("%s request time %u ms, Cmnd: %02X\r\n", TAG, _lastBatteryCheck.elapsed(), _lastCmnd);
}

static void errorBatteryNotResponding()
{
    MessageOutput.printf("%s Battery not responding, check cabling or battery power switch\r\n", TAG);
}

static const char* _Function_[] = { "REQUEST", "REQUEST & GET", "GET" };
void PylontechRS485Receiver::get_protocol_version(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetProtocolVersion, 0);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f;
    if ((f = read_frame()) == NULL) {
        errorBatteryNotResponding();
        return;
    };

    if (_verboseLogging)
        MessageOutput.printf("%s Protocol Version: %d.%d\r\n", TAG, (f->ver & 0xF0) >> 4, f->ver & 0xF);
}

void PylontechRS485Receiver::get_barcode(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetBarCode);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }
}

void PylontechRS485Receiver::get_firmware_info(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetFirmwareInfo);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID; // point to index in Battery array

    uint8_t CommandValue = f->info[0];
    uint8_t* info = &f->info[1];

    // point to battery pack structure of selected battery
    PylontechRS485BatteryStats::Pack_t& Pack = _stats->Pack[module];

    uint32_t version = ToUint16(info);
    Pack.manufacturerVersion = String((version >> 8) & 0x0F) + '.' + String(version & 0x0F);
    version = ToUint24(info);
    Pack.mainLineVersion = String((version >> 16) & 0x0F) + '.' + String((version >> 8) & 0x0F) + '.' + String(version & 0x0F);

    if (_verboseLogging) {
        MessageOutput.printf("%s CommandValue: %d, Manufacturer Version: '%s'\r\n", TAG, CommandValue, Pack.manufacturerVersion.c_str());
        MessageOutput.printf("%s Main Line Version: '%s'\r\n", TAG, Pack.mainLineVersion.c_str());
    }
}

void PylontechRS485Receiver::get_version_info(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetVersionInfo);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }
}

bool PylontechRS485Receiver::get_pack_count(const PylontechRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d InfoCommand %02X\r\n", TAG, __FUNCTION__,
            _Function_[function], module, InfoCommand == 0 ? 0 : InfoCommand == 1 ? module
                                                                                  : 0xFF);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetPackCount, InfoCommand);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return true;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return false;
    }

    _stats->_number_of_packs = f->info[0];
    if (_stats->_number_of_packs > MAX_BATTERIES)
        _stats->_number_of_packs = MAX_BATTERIES; // currently limited to max 2 batteries (see default.h)
    if (_stats->_number_of_packs > Configuration.get().Battery.numberOfBatteries)
        _stats->_number_of_packs = Configuration.get().Battery.numberOfBatteries;

    if (_verboseLogging)
        MessageOutput.printf("%s::%s Number of Battery Packs %u\r\n", TAG, __FUNCTION__, _stats->_number_of_packs);

    return true;
}

void PylontechRS485Receiver::get_manufacturer_info(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetManufacturerInfo, 0);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID; // point to index in Battery array

    // point to battery pack structure of selected battery
    PylontechRS485BatteryStats::Pack_t& Pack = _stats->Pack[module];

    uint8_t* info = &f->info[0];

    String deviceName(reinterpret_cast<char*>(info), 10);
    info += 10;
    uint16_t sw_version = ToUint16(info);
    Pack.softwareVersion = String((sw_version >> 8) & 0x0F) + '.' + String(sw_version & 0x0F);
    String manufacturer(reinterpret_cast<char*>(info), 20);

    if (_verboseLogging) {
        MessageOutput.printf("%s Manufacturer: '%s' size: %d\r\n", TAG, manufacturer.c_str(), manufacturer.length());
        MessageOutput.printf("%s Software Version: %s\r\n", TAG, Pack.softwareVersion.c_str());
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

    if (_verboseLogging) {
        MessageOutput.printf("%s Manufacturer: '%s' size: %d\r\n", TAG, manufacturer.c_str(), manufacturer.length());
        MessageOutput.printf("%s Device Name: '%s' size: %d\r\n", TAG, deviceName.c_str(), deviceName.length());
    }

    _stats->setManufacturer(std::move(manufacturer));
    Pack.deviceName = deviceName;

    if (_verboseLogging)
        MessageOutput.printf("%s Device Name: '%s' Manufacturer Name: '%s'\r\n", TAG, Pack.deviceName.c_str(), _stats->getManufacturer().c_str());
}

void PylontechRS485Receiver::get_analog_value(const PylontechRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d InfoCommand %02X\r\n", TAG, __FUNCTION__,
            _Function_[function], module, InfoCommand == 0 ? 0 : InfoCommand == 1 ? module
                                                                                  : 0xFF);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetAnalogValue, InfoCommand);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID;

    // point to battery pack structure of selected battery
    PylontechRS485BatteryStats::Pack_t& Pack = _stats->Pack[module];

    // INFO = INFOFLAG + DATAI
    uint8_t InfoFlag = f->info[0];
    uint8_t* info = &(f->info[1]);

    Pack.numberOfModule = *info++;
    //    Pack.numberOfModule = module+2;
    Pack.numberOfCells = min(*info++, static_cast<uint8_t>(15));

    if (_verboseLogging)
        MessageOutput.printf("%s InfoFlag: %02X, Number of Cells: %d\r\n", TAG, InfoFlag, Pack.numberOfCells);

    Pack.cellMinVoltage = 999999.0;
    Pack.cellMaxVoltage = -999999.0;
    Pack.CellVoltages = reinterpret_cast<float*>(realloc(Pack.CellVoltages, Pack.numberOfCells * sizeof(float)));
    for (int i = 0; i < Pack.numberOfCells; i++) {
        float value = to_CellVolt(info);
        Pack.CellVoltages[i] = value;
        Pack.cellMinVoltage = min(Pack.cellMinVoltage, value);
        Pack.cellMaxVoltage = max(Pack.cellMaxVoltage, value);
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        MessageOutput.printf("%s CellVoltage[%d]: %.3f\r\n", TAG, i + 1, Pack.CellVoltages[i]);
#endif
    }
    Pack.cellDiffVoltage = (Pack.cellMaxVoltage - Pack.cellMinVoltage) * 1000.0; // in mV

    Pack.numberOfTemperatures = min(*info++, static_cast<uint8_t>(6));

    if (_verboseLogging)
        MessageOutput.printf("%s Number of Temperatures: %d\r\n", TAG, Pack.numberOfTemperatures);

    Pack.averageBMSTemperature = to_Celsius(info);
    Pack.averageCellTemperature = 0.0;
    Pack.minCellTemperature = 10000.0;
    Pack.maxCellTemperature = -10000.0;
    Pack.GroupedCellsTemperatures = reinterpret_cast<float*>(realloc(Pack.GroupedCellsTemperatures, (Pack.numberOfTemperatures - 1) * sizeof(float)));
    for (int i = 0; i < Pack.numberOfTemperatures - 1; i++) {
        float t = to_Celsius(info);
        Pack.GroupedCellsTemperatures[i] = t;
        Pack.averageCellTemperature += t;
        Pack.minCellTemperature = min(Pack.minCellTemperature, t);
        Pack.maxCellTemperature = max(Pack.maxCellTemperature, t);
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        MessageOutput.printf("%s GroupedCellsTemperatures[%d]: %.1f\r\n", TAG, i + 1, Pack.GroupedCellsTemperatures[i]);
#endif
    }
    Pack.averageCellTemperature /= (Pack.numberOfTemperatures - 1);
    Pack.current = to_Amp(info);
    Pack.voltage = to_Volt(info);
    Pack.power = Pack.current * Pack.voltage / 1000.0; // kW
    Pack.remainingCapacity = DivideUint16By1000(info); // DivideBy1000(Int16ub),
    uint8_t _UserDefinedItems = *info++;
    Pack.capacity = DivideUint16By1000(info);
    Pack.cycles = ToUint16(info);
    if (_UserDefinedItems > 2) {
        Pack.remainingCapacity = DivideUint24By1000(info);
        Pack.capacity = DivideUint24By1000(info);
    }
    Pack.SoC = (100.0 * Pack.remainingCapacity) / Pack.capacity;

    // point to battery totals structure of selected battery
    PylontechRS485BatteryStats::Totals_t& totals = _stats->totals;

    totals.voltage = -99999.0;
    totals.power = 0.0f;
    totals.current = 0.0f;
    totals.capacity = 0.0f;
    totals.remainingCapacity = 0.0f;
    totals.averageBMSTemperature = 0.0f;
    totals.averageCellTemperature = 0.0f;
    totals.minCellTemperature = 10000.0;
    totals.maxCellTemperature = -10000.0;
    totals.cellMinVoltage = 10000.0;
    totals.cellMaxVoltage = -10000.0;
    totals.cycles = 0;

    // build total values over the battery pack
    for (uint8_t i = 0; i < _stats->_number_of_packs; i++) {
        totals.voltage = max(totals.voltage, _stats->Pack[i].voltage);
        totals.power += _stats->Pack[i].power;
        totals.current += _stats->Pack[i].current;
        totals.capacity += _stats->Pack[i].capacity;
        totals.remainingCapacity += _stats->Pack[i].remainingCapacity;
        totals.cycles = max(totals.cycles, _stats->Pack[i].cycles);

        totals.cellMinVoltage = min(totals.cellMinVoltage, _stats->Pack[i].cellMinVoltage);
        totals.cellMaxVoltage = max(totals.cellMaxVoltage, _stats->Pack[i].cellMaxVoltage);

        totals.averageBMSTemperature = max(totals.averageBMSTemperature, _stats->Pack[i].averageBMSTemperature);
        totals.averageCellTemperature = max(totals.averageCellTemperature, _stats->Pack[i].averageCellTemperature);
        totals.minCellTemperature = min(totals.minCellTemperature, _stats->Pack[i].minCellTemperature);
        totals.maxCellTemperature = max(totals.maxCellTemperature, _stats->Pack[i].maxCellTemperature);
    }
    _stats->setVoltage(totals.voltage, millis());
    _stats->setCurrent(totals.current, 1/*precision*/, millis());
    totals.cellDiffVoltage = (totals.cellMaxVoltage - totals.cellMinVoltage) * 1000.0; // in mV
    totals.SoC = (100.0 * totals.remainingCapacity) / totals.capacity;
    _stats->setSoC(totals.SoC, 1 /*precision*/, millis());

    if (_verboseLogging) {
        MessageOutput.printf("%s AverageBMSTemperature: %.1f\r\n", TAG, Pack.averageBMSTemperature);
        MessageOutput.printf("%s AverageCellTemperature: %.1f\r\n", TAG, Pack.averageCellTemperature);
        MessageOutput.printf("%s MinCellTemperature: %.1f\r\n", TAG, Pack.minCellTemperature);
        MessageOutput.printf("%s MaxCellTemperature: %.1f\r\n", TAG, Pack.maxCellTemperature);
        MessageOutput.printf("%s Current: %.1f, Voltage: %.3f\r\n", TAG, Pack.current, _stats->getVoltage());
        MessageOutput.printf("%s Capacity: %.3f, Remaining: %.3f\r\n", TAG, Pack.capacity, Pack.remainingCapacity);
        MessageOutput.printf("%s Power: %.3f kW\r\n", TAG, Pack.power);
        MessageOutput.printf("%s State of Charge: %.1f%%, Cycles: %d\r\n", TAG, _stats->getSoC(), Pack.cycles);
    }
}

void PylontechRS485Receiver::get_system_parameters(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetSystemParameter, 0);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID;

    // INFO = INFOFLAG + DATAI
    uint8_t InfoFlag = f->info[0];
    uint8_t* info = &(f->info[1]);

    // point to Systemparameter structure of selected battery
    SystemParameters_t& SystemParameters = _stats->Pack[module].SystemParameters;

    SystemParameters.cellHighVoltageLimit = to_Volt(info);
    SystemParameters.cellLowVoltageLimit = to_Volt(info);
    SystemParameters.cellUnderVoltageLimit = to_Volt(info);
    SystemParameters.chargeHighTemperatureLimit = to_Celsius(info);
    SystemParameters.chargeLowTemperatureLimit = to_Celsius(info);
    SystemParameters.chargeCurrentLimit = to_Amp(info);
    SystemParameters.moduleHighVoltageLimit = to_Volt(info);
    SystemParameters.moduleLowVoltageLimit = to_Volt(info);
    SystemParameters.moduleUnderVoltageLimit = to_Volt(info);
    SystemParameters.dischargeHighTemperatureLimit = to_Celsius(info);
    SystemParameters.dischargeLowTemperatureLimit = to_Celsius(info);
    SystemParameters.dischargeCurrentLimit = to_Amp(info);

    // build totals over the battery pack
    _stats->totals.SystemParameters.chargeCurrentLimit = 0;
    _stats->totals.SystemParameters.dischargeCurrentLimit = 0;
    _stats->totals.SystemParameters.chargeLowTemperatureLimit = -999999.0;
    _stats->totals.SystemParameters.chargeHighTemperatureLimit = 999999.0;
    _stats->totals.SystemParameters.dischargeLowTemperatureLimit = -999999.0;
    _stats->totals.SystemParameters.dischargeHighTemperatureLimit = 999999.0;
    for (uint8_t i = 0; i < _stats->_number_of_packs; i++) {
        _stats->totals.SystemParameters.chargeCurrentLimit += _stats->Pack[i].SystemParameters.chargeCurrentLimit;
        _stats->totals.SystemParameters.dischargeCurrentLimit += _stats->Pack[i].SystemParameters.dischargeCurrentLimit;
        _stats->totals.SystemParameters.chargeLowTemperatureLimit = max(_stats->totals.SystemParameters.chargeLowTemperatureLimit, _stats->Pack[i].SystemParameters.chargeLowTemperatureLimit);
        _stats->totals.SystemParameters.chargeHighTemperatureLimit = min(_stats->totals.SystemParameters.chargeHighTemperatureLimit, _stats->Pack[i].SystemParameters.chargeHighTemperatureLimit);
        _stats->totals.SystemParameters.dischargeLowTemperatureLimit = max(_stats->totals.SystemParameters.dischargeLowTemperatureLimit, _stats->Pack[i].SystemParameters.dischargeLowTemperatureLimit);
        _stats->totals.SystemParameters.dischargeHighTemperatureLimit = min(_stats->totals.SystemParameters.dischargeHighTemperatureLimit, _stats->Pack[i].SystemParameters.dischargeHighTemperatureLimit);
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s InfoFlag: %02X, CurrentLimit charge: %.2fA, discharge: %.2fA\r\n", TAG,
            InfoFlag,
            SystemParameters.chargeCurrentLimit,
            SystemParameters.dischargeCurrentLimit);
        MessageOutput.printf("%s Cell VoltageLimit High: %.3fV, Low: %.3fV, Under: %.3fV\r\n", TAG,
            SystemParameters.cellHighVoltageLimit,
            SystemParameters.cellLowVoltageLimit,
            SystemParameters.cellUnderVoltageLimit);
        MessageOutput.printf("%s Charge TemperatureLimit High: %.1f°C, Low: %.1f°C\r\n", TAG,
            SystemParameters.chargeHighTemperatureLimit,
            SystemParameters.chargeLowTemperatureLimit);
        MessageOutput.printf("%s VoltageLimit High: %.3fV, Low: %.3fV, Under: %.3fV\r\n", TAG,
            SystemParameters.moduleHighVoltageLimit,
            SystemParameters.moduleLowVoltageLimit,
            SystemParameters.moduleUnderVoltageLimit);
        MessageOutput.printf("%s Discharge TemperatureLimit High: %.1f°C, Low: %.1f°C\r\n", TAG,
            SystemParameters.dischargeHighTemperatureLimit,
            SystemParameters.dischargeLowTemperatureLimit);
    }
}

void PylontechRS485Receiver::get_alarm_info(const PylontechRS485Receiver::Function function, uint8_t module, uint8_t InfoCommand)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d InfoCommand %02X\r\n", TAG, __FUNCTION__,
            _Function_[function], module, InfoCommand == 0 ? 0 : InfoCommand == 1 ? module
                                                                                  : 0xFF);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetAlarmInfo, InfoCommand);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID;

    // INFO = DATAFLAG + WARNSTATE
    uint8_t DataFlag = f->info[0];
    uint8_t CommandValue = f->info[1];
    uint8_t* info = &f->info[2];

    // point to Alarm and Warning structure of selected battery
    Alarm_t& Alarm = _stats->Pack[module].Alarm;
    Warning_t& Warning = _stats->Pack[module].Warning;

    // structure of Pylontech RS485 response
    AlarmInfo_t AlarmInfo;

    AlarmInfo.numberOfCells = min(*info++, static_cast<uint8_t>(15));
    Warning.lowVoltage = 0;
    Warning.highVoltage = 0;

    char buffer[AlarmInfo.numberOfCells * 3 + 1];
    for (int i = 0; i < AlarmInfo.numberOfCells; i++) {
        AlarmInfo.cellVoltages[i] = *info++;

        if (AlarmInfo.cellVoltages[i] & 0x01) Warning.lowVoltage = 1;
        if (AlarmInfo.cellVoltages[i] & 0x02) Warning.highVoltage = 1;

        if (_verboseLogging)
            snprintf(&buffer[i * 3], 4, "%02X ", AlarmInfo.cellVoltages[i]);
    }
    if (_verboseLogging)
        MessageOutput.printf("%s DataFlag: %02X, CommandValue: %02X, Number of Cell: %d, Status: %s\r\n", TAG,
            DataFlag,
            CommandValue,
            AlarmInfo.numberOfCells, buffer);

    AlarmInfo.numberOfTemperatures = min(*info++, static_cast<uint8_t>(6));
    Warning.lowTemperature = 0; // set to normal
    Warning.highTemperature = 0; // set to normal

    AlarmInfo.BMSTemperature = *info++;
    if (AlarmInfo.BMSTemperature & 0x01) Warning.lowTemperature = 1;
    if (AlarmInfo.BMSTemperature & 0x02) Warning.highTemperature = 1;

    for (int i = 0; i < _stats->Pack[module].numberOfTemperatures - 1; i++) {
        AlarmInfo.Temperatures[i] = *info++;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
        snprintf(&buffer[i * 3], 4, "%02X ", AlarmInfo.Temperatures[i]);
#endif
        if (AlarmInfo.Temperatures[i] & 0x01) Warning.lowTemperature = 1;
        if (AlarmInfo.Temperatures[i] & 0x02) Warning.highTemperature = 1;
    }
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s Number of Temperature Sensors: %d,  Status: %s\r\n", TAG, AlarmInfo.NumberOfTemperatures, buffer);
#endif

    AlarmInfo.chargeCurrent = *info++;
    Warning.highCurrentCharge = (AlarmInfo.chargeCurrent & 0x02) ? 1 : 0;

    AlarmInfo.moduleVoltage = *info++;
    if (AlarmInfo.moduleVoltage & 0x01) Warning.lowVoltage = 1;
    if (AlarmInfo.moduleVoltage & 0x02) Warning.highVoltage = 1;

    AlarmInfo.dischargeCurrent = *info++;
    Warning.highCurrentDischarge = (AlarmInfo.dischargeCurrent & 0x02) ? 1 : 0;

    AlarmInfo.status1 = *info++;
    Alarm.underVoltage = AlarmInfo.moduleUnderVoltage;
    Alarm.overVoltage = AlarmInfo.moduleOverVoltage;
    Alarm.overCurrentCharge = AlarmInfo.chargeOverCurrent;
    Alarm.overCurrentDischarge = AlarmInfo.dischargeOverCurrent;
    Alarm.underTemperature = _stats->Pack[module].averageBMSTemperature < _stats->Pack[module].SystemParameters.dischargeLowTemperatureLimit;
    Alarm.overTemperature = AlarmInfo.dischargeOverTemperature | AlarmInfo.chargeOverTemperature;

    AlarmInfo.status2 = *info++;
    AlarmInfo.status3 = *info++;
    AlarmInfo.status4 = *info++;
    AlarmInfo.status5 = *info;

    for (int i = 0; i < AlarmInfo.numberOfCells; i++) {
        if ((AlarmInfo.cellVoltages[i] & 0x01) && (AlarmInfo.cellError & (1 << i)))
            Alarm.underVoltage = 1;
        if ((AlarmInfo.cellVoltages[i] & 0x02) && (AlarmInfo.cellError & (1 << i)))
            Alarm.overVoltage = 1;
    }

    // build total alarms and warnings over all batteries
    _stats->totals.Alarm.alarms = _stats->totals.Warning.warnings = 0;
    for (uint8_t i = 0; i < _stats->_number_of_packs; i++) {
        _stats->totals.Alarm.alarms |= _stats->Pack[i].Alarm.alarms;
        _stats->totals.Warning.warnings |= _stats->Pack[i].Warning.warnings;
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s Status ChargeCurrent: %02X ModuleVoltage: %02X DischargeCurrent: %02X\r\n", TAG,
            AlarmInfo.chargeCurrent, AlarmInfo.moduleVoltage, AlarmInfo.dischargeCurrent);
        MessageOutput.printf("%s Status 1: MOV:%s, CUV:%s, COC:%s, DOC:%s, DOT:%s, COT:%s, MUV:%s\r\n", TAG,
            AlarmInfo.moduleOverVoltage ? "trigger" : "normal",
            AlarmInfo.cellUnderVoltage ? "trigger" : "normal",
            AlarmInfo.chargeOverCurrent ? "trigger" : "normal",
            AlarmInfo.dischargeOverCurrent ? "trigger" : "normal",
            AlarmInfo.dischargeOverTemperature ? "trigger" : "normal",
            AlarmInfo.chargeOverTemperature ? "trigger" : "normal",
            AlarmInfo.moduleUnderVoltage ? "trigger" : "normal");
        MessageOutput.printf("%s Status 2: %s using battery module power, Discharge MOSFET %s, Charge MOSFET %s, Pre MOSFET %s\r\n", TAG,
            AlarmInfo.usingBatteryModulePower ? "" : "not",
            AlarmInfo.dischargeMOSFET ? "ON" : "OFF",
            AlarmInfo.chargeMOSFET ? "ON" : "OFF",
            AlarmInfo.preMOSFET ? "ON" : "OFF");
        MessageOutput.printf("%s Status 3: buzzer %s, fully Charged (SOC=%s), discharge current detected by BMS %s, charge current detected by BMS %s\r\n", TAG,
            AlarmInfo.buzzer ? "on" : "off",
            AlarmInfo.fullyCharged ? "100%" : "normal",
            AlarmInfo.effectiveDischargeCurrent ? ">0.1A" : "normal",
            AlarmInfo.effectiveChargeCurrent ? "<-0.1A" : "normal");
        MessageOutput.printf("%s Cell Error: %04X\r\n", TAG, AlarmInfo.cellError);
    }
    /*
        Alarm.BmsInternal =
    */
}

void PylontechRS485Receiver::get_charge_discharge_management_info(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetChargeDischargeManagementInfo);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID;

    // INFO = DATAI
    uint8_t CommandValue = f->info[0];
    uint8_t* info = &(f->info[1]);

    ChargeDischargeManagementInfo_t& ChargeDischargeManagementInfo = _stats->Pack[module].ChargeDischargeManagementInfo;

    ChargeDischargeManagementInfo.chargeVoltageLimit = to_Volt(info);
    ChargeDischargeManagementInfo.dischargeVoltageLimit = to_Volt(info);
    ChargeDischargeManagementInfo.chargeCurrentLimit = to_Amp(info);
    ChargeDischargeManagementInfo.dischargeCurrentLimit = to_Amp(info);
    ChargeDischargeManagementInfo.Status = *info;

    // build totals over all batteries
    _stats->totals.ChargeDischargeManagementInfo.chargeVoltageLimit = 10000.;
    _stats->totals.ChargeDischargeManagementInfo.dischargeVoltageLimit = -10000.;
    _stats->totals.ChargeDischargeManagementInfo.chargeCurrentLimit = 0;
    _stats->totals.ChargeDischargeManagementInfo.dischargeCurrentLimit = 0;
    _stats->totals.ChargeDischargeManagementInfo.Status = 0;
    for (uint8_t i = 0; i < _stats->_number_of_packs; i++) {
        _stats->totals.ChargeDischargeManagementInfo.chargeCurrentLimit += _stats->Pack[i].ChargeDischargeManagementInfo.chargeCurrentLimit;
        _stats->totals.ChargeDischargeManagementInfo.dischargeCurrentLimit += _stats->Pack[i].ChargeDischargeManagementInfo.dischargeCurrentLimit;
        _stats->totals.ChargeDischargeManagementInfo.chargeVoltageLimit = min(_stats->totals.ChargeDischargeManagementInfo.chargeVoltageLimit, ChargeDischargeManagementInfo.chargeVoltageLimit);
        _stats->totals.ChargeDischargeManagementInfo.dischargeVoltageLimit = max(_stats->totals.ChargeDischargeManagementInfo.dischargeVoltageLimit, ChargeDischargeManagementInfo.dischargeVoltageLimit);
        _stats->totals.ChargeDischargeManagementInfo.Status |= _stats->Pack[i].ChargeDischargeManagementInfo.Status;
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s CommandValue: %02X, CVL:%.3fV, DVL:%.3fV, CCL:%.1fA, DCL:%.1fA, CE:%d DE:%d CI1:%d CI2:%d FCR:%d\r\n", TAG,
            CommandValue,
            ChargeDischargeManagementInfo.chargeVoltageLimit,
            ChargeDischargeManagementInfo.dischargeVoltageLimit,
            ChargeDischargeManagementInfo.chargeCurrentLimit,
            ChargeDischargeManagementInfo.dischargeCurrentLimit,
            ChargeDischargeManagementInfo.chargeEnabled,
            ChargeDischargeManagementInfo.dischargeEnabled,
            ChargeDischargeManagementInfo.chargeImmediately1,
            ChargeDischargeManagementInfo.chargeImmediately2,
            ChargeDischargeManagementInfo.fullChargeRequest);
    }
}

void PylontechRS485Receiver::get_module_serial_number(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::GetSerialNumber);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    format_t* f = read_frame();
    if (f == NULL) {
        errorBatteryNotResponding();
        return;
    }

    module -= _masterBatteryID;

    ModuleSerialNumber_t& ModuleSerialNumber = _stats->Pack[module].ModuleSerialNumber;

    // INFO = DATAI
    uint8_t CommandValue = f->info[0];
    ModuleSerialNumber.moduleSerialNumber[16] = 0;
    strncpy(ModuleSerialNumber.moduleSerialNumber, (const char*)&(f->info[1]), sizeof(ModuleSerialNumber.moduleSerialNumber) - 1);

    if (_verboseLogging)
        MessageOutput.printf("%s CommandValue %02X, S/N '%s'\r\n", TAG, CommandValue, ModuleSerialNumber.moduleSerialNumber);
}

void PylontechRS485Receiver::setting_charge_discharge_management_info(const PylontechRS485Receiver::Function function, uint8_t module, const char* info)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::SetChargeDischargeManagementInfo);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }
}

void PylontechRS485Receiver::turn_off_module(const PylontechRS485Receiver::Function function, uint8_t module)
{
    if (_verboseLogging)
        MessageOutput.printf("%s::%s %s Module %d\r\n", TAG, __FUNCTION__, _Function_[function], module);

    if (function != PylontechRS485Receiver::Function::GET) {
        send_cmd(module, Command::TurnOffModule);
        yield();
        if (function == PylontechRS485Receiver::Function::REQUEST)
            return;
        else {
            vTaskDelay(100);
            // and fall through to get the data from the battery
        }
    }

    /*format_t *f =*/read_frame();
}

uint16_t PylontechRS485Receiver::get_frame_checksum(char* frame)
{
    uint16_t sum = 0;

    while (*frame) sum += *frame++;
    sum = ~sum;
    sum %= 0x10000;
    sum += 1;

    return sum;
}

uint16_t PylontechRS485Receiver::get_info_length(const char* info)
{
    uint8_t lenid = strlen(info);
    if (lenid == 0) return 0;

    uint8_t lenid_sum = (lenid & 0xf) + ((lenid >> 4) & 0xf) + ((lenid >> 8) & 0xf);
    uint8_t lenid_modulo = lenid_sum % 16;
    uint8_t lenid_invert_plus_one = 0b1111 - lenid_modulo + 1;

    uint16_t length = (lenid_invert_plus_one << 12) + lenid;
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s info length: %04X\r\n", TAG, length);
#endif

    return length;
}

void PylontechRS485Receiver::_encode_cmd(char* frame, uint8_t address, uint8_t cid2, const char* info)
{
    char subframe[26];
    const uint8_t cid1 = 0x46;

    snprintf(subframe, sizeof(subframe), "%02X%02X%02X%02X%04X%s", 0x20, address, cid1, cid2, get_info_length(info), info);
    snprintf(frame, 32, "%c%s%04X%c", 0x7E, subframe, get_frame_checksum(subframe), 0x0D);
}

void PylontechRS485Receiver::send_cmd(uint8_t address, uint8_t cmnd, uint8_t InfoCommand)
{
    char raw_frame[32];
    char bdevid[3] = "";

    if (InfoCommand) snprintf(bdevid, sizeof(bdevid), "%02X", InfoCommand == 1 ? address : 0xFF);
    _encode_cmd(raw_frame, address, cmnd, bdevid);
    size_t length = strlen(raw_frame);

    if (_verboseLogging)
        MessageOutput.printf("%s:%s frame=%s\r\n", TAG, __FUNCTION__, raw_frame);

    if (_upSerial->write(raw_frame, length) != length) {
        MessageOutput.printf("%s Send data critical failure.\r\n", TAG);
        // add your code to handle sending failure here
        //        abort();
    }
    _lastCmnd = cmnd;
}

size_t PylontechRS485Receiver::readline(void)
{
    uint32_t t_end = millis() + 2000; // timeout 2 seconds
    while (millis() < t_end) {
        if (_upSerial->available()) break;
        yield();
    }
    if (!_upSerial->available()) {
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
        if (_upSerial->available()) {
            c = _upSerial->read();
            if (!start && c != 0x7E) { // start of line ?
                yield();
                continue;
            }

            start = true;

            static int _receivedFrameSize = 128;
            if (((idx % 32) == 0) && (idx >= _receivedFrameSize) && (_receivedFrameSize < 256)) {
                char* prev;
                _received_frame = reinterpret_cast<char*>(realloc(prev = _received_frame, _receivedFrameSize + 32));
                _receivedFrameSize += 32;
                MessageOutput.printf("%s %s realloc location: %p. Size: %d bytes.\r\n", TAG,
                    prev != _received_frame ? "New" : "Old", (void*)_received_frame, _receivedFrameSize);
            }
            _received_frame[idx++] = c;

            if (c == 0x0D) { // end of line ?
                _received_frame[idx] = 0;
                break;
            }

            if (idx > 255) {
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

    if (len == 0) return 0;

    do {
        if      (*s >= '0' && *s <= '9') b |= *s - '0';
        else if (*s >= 'A' && *s <= 'F') b |= *s - 'A' + 10;
        else if (*s >= 'a' && *s <= 'f') b |= *s - 'a' + 10;
        if (len > 1) b <<= 4;
        s++;
    } while (--len);

    return b;
}

char* PylontechRS485Receiver::_decode_hw_frame(size_t length)
{
#ifdef PYLONTECH_RS485_DEBUG_ENABLED
    MessageOutput.printf("%s _received_frame '%s' length=%d\r\n", TAG, _received_frame, length);
#endif
    if (length < 12) return NULL;

    char* frame_data = &_received_frame[1]; // remove SOI 0x7E
    _received_frame[length - 1] = 0; // remove EOI 0x0D
    length -= 2;

    uint16_t read_chksum = strtol(&frame_data[length - 4], NULL, 16);
    frame_data[length - 4] = 0; // remove frame checksum
    length -= 4;

    uint16_t computed_chksum = get_frame_checksum(frame_data);

    if (_verboseLogging) {
        MessageOutput.printf("%s frame_data '%s' length=%d\r\n", TAG, frame_data, length);
        MessageOutput.printf("%s frame_chksum read=%04X, computed=%04X\r\n", TAG, read_chksum, computed_chksum);
    }

    // frame checksum not correct, discard complete frame
    if (read_chksum != computed_chksum) return NULL;

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

    if (_verboseLogging)
        MessageOutput.printf("%s ver: %02X, adr: %02X, cid1: %02X, cid2: %02X, infolength: %d info:\r\n", TAG, f->ver, f->adr, f->cid1, f->cid2, (f->infolength) & 0xFFF);

    for (int idx = 12, pos = 0; idx < length; idx += 2, pos++) {
        f->info[pos] = (uint8_t)hex2binary(&frame[idx], 2);
        if (_verboseLogging) {
            MessageOutput.printf(" %02X", f->info[pos]);
            if (((pos + 1) % 16) == 0) MessageOutput.println();
        }
    }
    if (_verboseLogging) MessageOutput.println();

    return f;
}

format_t* PylontechRS485Receiver::read_frame(void)
{
    size_t length;

    if (!(length = readline()))
        return NULL; // nothing read, timeout

    char* frame = _decode_hw_frame(length);
    if (frame == NULL) return NULL;

    return _decode_frame(frame);
}

#endif
