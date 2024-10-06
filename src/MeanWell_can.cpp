// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 - 2024 Andreas Högner
 */
#ifdef USE_CHARGER_MEANWELL

#define MEANWELL_DEBUG_ENABLED
// #define MEANWELL_DEBUG_ENABLED__

#include <Arduino.h>
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "Battery.h"
#include "SunPosition.h"
#include "PowerMeter.h"
#include "Configuration.h"
#include <Hoymiles.h>
#include <math.h>
#include <AsyncJson.h>
#include "SpiManager.h"

#include <Preferences.h>
Preferences preferences;

MeanWellCanClass MeanWellCan;

SemaphoreHandle_t xSemaphore = NULL;

MeanWellCanClass::MeanWellCanClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MeanWellCanClass::loop, this))
{
}

static constexpr char sEEPROMwrites[] = "EEPROMwrites";

void MeanWellCanClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize MeanWell AC charger interface... ");

   snprintf(_providerName, sizeof(_providerName), "[%s %s]", "Meanwell", PinMapping.get().charger.providerName);

    _previousMillis = millis();

    preferences.begin("OpenDTU", false);
    if (!preferences.isKey(sEEPROMwrites)) {
        MessageOutput.printf("create ");
        preferences.putULong(sEEPROMwrites, 0);
    }
    EEPROMwrites = preferences.getULong(sEEPROMwrites);
    MessageOutput.printf("%s = %u, ", sEEPROMwrites, EEPROMwrites);

    if (xSemaphore == NULL)
        xSemaphore = xSemaphoreCreateMutex();

    scheduler.addTask(_loopTask);

    if (!Configuration.get().MeanWell.Enabled) {
        MessageOutput.println("not enabled");
        return;
    }

    updateSettings();

    MessageOutput.println("done");
}

void MeanWellCanClass::updateSettings()
{
    preferences.putULong(sEEPROMwrites, EEPROMwrites);

    auto const& config = Configuration.get();
    _verboseLogging = config.MeanWell.VerboseLogging;

    if (!config.MeanWell.Enabled) {
        _loopTask.disable();
        return;
    }

    if (_initialized) {
        _setupParameter = true;
        return;
    }

    if (!PinMapping.isValidChargerConfig()) {
        MessageOutput.println("Invalid pin config.");
        return;
    }

    auto const& pin = PinMapping.get().charger;
    switch (pin.provider) {
#ifdef USE_CHARGER_CAN0
        case Charger_Provider_t::CAN0:
            {

            auto tx = static_cast<gpio_num_t>(pin.can0.tx);
            auto rx = static_cast<gpio_num_t>(pin.can0.rx);

            MessageOutput.printf("CAN0 port rx = %d, tx = %d.\r\n", rx, tx);

            twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);
            // g_config.bus_off_io = (gpio_num_t)can0_stb;
#if defined(BOARD_HAS_PSRAM)
            g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;
#endif
            twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
            twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

            // Install TWAI driver
            MessageOutput.print("Twai driver install");
            switch (twai_driver_install(&g_config, &t_config, &f_config)) {
                case ESP_OK:
                    MessageOutput.print("ed");
                    break;
                case ESP_ERR_INVALID_ARG:
                    MessageOutput.println(" - invalid arg.");
                    return;
                case ESP_ERR_NO_MEM:
                    MessageOutput.println(" - no memory.");
                    return;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.println(" - invalid state.");
                    return;
                default:;
                    MessageOutput.println(" failed.");
                    return;
            }

            // Start TWAI driver
            MessageOutput.print(", start");
            switch (twai_start()) {
                case ESP_OK:
                    MessageOutput.println("ed.");
                    break;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.println(" - invalid state.");
                    return;
                default:;
                    MessageOutput.println(" failed.");
                    return;
            }
            }
            break;
#endif
#ifdef USE_CHARGER_I2C
        case Charger_Provider_t::I2C0:
        case Charger_Provider_t::I2C1:
            {
            MessageOutput.printf("I2C CAN Bus @ I2C%d scl = %d, sda = %d.\r\n", pin.provider==Charger_Provider_t::I2C0?0:1, pin.i2c.scl, pin.i2c.sda);

            i2c_can = new I2C_CAN(pin.provider == Charger_Provider_t::I2C0?&Wire:&Wire1, 0x25, pin.i2c.scl, pin.i2c.sda, 400000UL);     // Set I2C Address

            int i = 10;
            while (CAN_OK != i2c_can->begin(I2C_CAN_250KBPS))    // init can bus : baudrate = 250k
            {
                delay(200);
                if (--i) continue;

                MessageOutput.println("CAN Bus FAIL!");
                return;
            }

            MessageOutput.println("I2C CAN Bus OK!");
            }
            break;
#endif
#ifdef USE_CHARGER_MCP2515
        case Charger_Provider_t::MCP2515:
            {
            int rc;
            MessageOutput.printf("MCP2515 CAN: miso = %d, mosi = %d, clk = %d, cs = %d, irq = %d.\r\n",
                                pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.clk, pin.mcp2515.cs, pin.mcp2515.irq);

            CAN = new MCP2515Class(pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.clk, pin.mcp2515.cs, pin.mcp2515.irq);

            auto frequency = config.MCP2515.Controller_Frequency;
	        auto mcp_frequency = MCP_8MHZ;
	        if (20000000UL == frequency) { mcp_frequency = MCP_20MHZ; }
    	    else if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
            else if (8000000UL != frequency) {
                MessageOutput.printf("MCP2515 CAN: Unknown frequency %d Hz, using 8 MHz\r\n", mcp_frequency);
    	    }
            MessageOutput.printf("MCP2515 CAN: Quarz = %u Mhz\r\n", (unsigned int)(frequency/1000000UL));

            if ((rc = CAN->initMCP2515(MCP_ANY, CAN_250KBPS, mcp_frequency)) != CAN_OK) {
                MessageOutput.printf("%s MCP2515 failed to initialize. Error code: %d\r\n", _providerName, rc);
                return;
            };

	        // Change to normal mode to allow messages to be transmitted
	        if ((rc = CAN->setMode(MCP_NORMAL)) != CAN_OK) {
                MessageOutput.printf("%s MCP2515 failed to set mode to NORMAL. Error code: %d\r\n", _providerName, rc);
                return;
            }
            }
            break;
#endif
        default:;
            MessageOutput.println(" Error: no IO provider configured");
            return;
    }

    _loopTask.enable();

    _initialized = true;

    MessageOutput.println("Initialized Successfully!");
}

bool MeanWellCanClass::getCanCharger(void)
{
    bool rc = false;

    uint32_t t = millis();
    while (millis() - t < 750) { // timeout 750ms
        rc = parseCanPackets();
        if (rc == true)
            break;
    }

    return rc;
}

bool MeanWellCanClass::parseCanPackets(void)
{
    if (xSemaphore == NULL) {
        MessageOutput.printf("%s xSemapore not initialized\r\n", _providerName);
        return false;
    }

      auto _provider = PinMapping.get().charger.provider;

    /* See if we can obtain the semaphore.  If the semaphore is not
       available wait 1000 ticks to see if it becomes free. */
    if (xSemaphoreTake(xSemaphore, (TickType_t)1000) == pdTRUE) {
        twai_message_t rx_message = {};

        switch (_provider) {
#ifdef USE_CHARGER_CAN0
            case Charger_Provider_t::CAN0:
                {
                // Check for messages. twai_recive is blocking when there is no data so we return if there are no frames in the buffer
                twai_status_info_t status_info;
                if (twai_get_status_info(&status_info) != ESP_OK) {
                    MessageOutput.printf("%s CAN Failed to get Twai status info\r\n", _providerName);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }
                if (status_info.msgs_to_rx == 0) {
                    //  MessageOutput.printf("%s CAN no message received\r\n", _providerName);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                // Wait for message to be received, function is blocking
                if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
                    MessageOutput.printf("%s CAN Failed to receive message\r\n", _providerName);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }
                }
                break;
#endif
#ifdef USE_CHARGER_I2C
            case Charger_Provider_t::I2C0:
            case Charger_Provider_t::I2C1:
                if (CAN_MSGAVAIL != i2c_can->checkReceive()) {
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                if (CAN_OK != i2c_can->readMsgBuf(&rx_message.data_length_code, rx_message.data)) {    // read data,  len: data length, buf: data buf
                    MessageOutput.printf("%s CAN nothing received\r\n", _providerName);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                if (rx_message.data_length_code > 8) {
                    MessageOutput.printf("%s CAN received %d bytes\r\n", _providerName, rx_message.data_length_code);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                if(rx_message.data_length_code == 0) {
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                rx_message.identifier = i2c_can->getCanId();
                rx_message.extd = i2c_can->isExtendedFrame();
                rx_message.rtr = i2c_can->isRemoteRequest();
                break;
#endif
#ifdef USE_CHARGER_MCP2515
            case Charger_Provider_t::MCP2515:
                {
                if (!CAN->isInterrupt()) {
                    //  MessageOutput.printf("%s CAN no message received\r\n", _providerName);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }
                // If CAN_INT pin is low, read receive buffer

                int rc;
                if ((rc = CAN->readMsgBuf(reinterpret_cast<can_message_t*>(&rx_message))) != CAN_OK) { // Read data: len = data length, buf = data byte(s)
                        MessageOutput.printf("%s failed to read CAN message: Error code %d\r\n", _providerName, rc);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }

                if (_verboseLogging) {
                    if(rx_message.extd)     // Determine if ID is standard (11 bits) or extended (29 bits)
                        MessageOutput.printf("Extended ID: 0x%.8" PRIx32 " DLC: %1d  Data:", rx_message.identifier & 0x1FFFFFFF, rx_message.data_length_code);
                    else
                    MessageOutput.printf("Standard ID: 0x%.3" PRIx32 " DLC: %1d  Data:", rx_message.identifier, rx_message.data_length_code);
                }
                if (rx_message.rtr){    // Determine if message is a remote request frame.
                    int len = rx_message.data_length_code>32?32:rx_message.data_length_code;
                    char data[len*3+1] = "";
                    for (uint8_t i=0; i<len; i++)
                        snprintf(&data[i*3], sizeof(data)-i*3, "%02X ", rx_message.data[i]);
                    data[strlen(data)] = 0;
                    MessageOutput.printf(" REMOTE REQUEST FRAME %s\r\n", data);
                    xSemaphoreGive(xSemaphore);
                    return false;
                }
                }
                break;
#endif
            default:;
        }

        _meanwellLastResponseTime = millis(); // save last response time
        yield();

        if (_verboseLogging)
            MessageOutput.printf("%s id: 0x%08X, extd: %d, data len: %d bytes\r\n", _providerName,
                rx_message.identifier, rx_message.extd, rx_message.data_length_code);

        switch (rx_message.identifier & 0xFFFFFF00) {
        case 0x000C0100:
            // MessageOutput.printf("%s 0x%08X len: %d\r\n", _providerName, rx_message.identifier, rx_message.data_length_code);
            xSemaphoreGive(xSemaphore);
            return false;
        case 0x000C0000:
            onReceive(rx_message.data, rx_message.data_length_code);
        }

        xSemaphoreGive(xSemaphore);
        return true;
    }

    MessageOutput.printf("%s xSemapore not free\r\n", _providerName);

    return false;
}

typedef struct {
    float x;
    float y;
} coord_t;

float MeanWellCanClass::calcEfficency(float x)
{
    int i;

    static coord_t c[] = {
        {    0.0f, 0.7500f },   // NPB-450   NPB-750  NPB-1200  NPB-1700
        {  100.0f, 0.9200f },   //  37.50W    62.50W   100.00W   141.67W
        {  177.0f, 0.9530f },   //  66.38W   110.63W   177.00W   250.75W
        {  222.0f, 0.9569f },   //  83.25W   138.75W   222.00W   314.50W
        {  440.0f, 0.9750f },   // 165.00W   275.00W   440.00W   623.33W
        {  666.0f, 0.9569f },   // 249.75W   416.25W   666.00W   943.50W
        {  888.0f, 0.9548f },   // 333.00W   555.00W   888.00W  1258.00W
        { 1000.0f, 0.9548f },   // 375.00W   625.00W  1000.00W  1416.67W
        { 1100.0f, 0.9525f },   // 412.50W   687.50W  1100.00W  1558.33W
        { 1300.0f, 0.9500f }    // 487.50W   812.50W  1300.00W  1841.67W
    };

    float scaling = 1.0f;
    switch (_model) {
    case NPB_Model_t::NPB_450_24:
    case NPB_Model_t::NPB_450_48:
        scaling = 450.0f / 1200.0f;
        break;
    case NPB_Model_t::NPB_750_24:
    case NPB_Model_t::NPB_750_48:
        scaling = 750.0f / 1200.0f;
        break;
    case NPB_Model_t::NPB_1200_24:
    case NPB_Model_t::NPB_1200_48:
        scaling = 1200.0f / 1200.0f;
        break;
    case NPB_Model_t::NPB_1700_24:
    case NPB_Model_t::NPB_1700_48:
        scaling = 1700.0f / 1200.0f;
        break;
    case NPB_Model_t::NPB_unknown:
        scaling = 1.0f;
        break;
    }

    for (i = 0; i < sizeof(c) / sizeof(coord_t) - 1; i++) {
        if (c[i].x * scaling <= x && c[i + 1].x * scaling >= x) {
            float diffx = x - c[i].x * scaling;
            float diffn = c[i + 1].x * scaling - c[i].x * scaling;

            return c[i].y + (c[i + 1].y - c[i].y) * diffx / diffn;
        }
    }

    return x < c[0].x * scaling ? c[0].y : c[i].y; // Not in Range
}

void MeanWellCanClass::calcPower() {
    _rp.outputPower = _rp.outputCurrent * _rp.outputVoltage;
    _rp.inputPower = _rp.outputPower / calcEfficency(_rp.outputPower) // efficiency of NPB-1200-48
                    + 4.0f                                            // self power of NPB-1200-48
                    + (0.75f * 240.0f / 1000.0f);                     // leakage power
    _rp.efficiency = (_rp.inputPower > 0.0f) ? 100.0f * _rp.outputPower / _rp.inputPower : 0.0f;
}

void MeanWellCanClass::onReceive(uint8_t* frame, uint8_t len)
{
    switch (readUnsignedInt16(frame)) { // parse Command
    case 0x0000: // OPERATION 1 byte ON/OFF control
        _rp.operation = *(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Operation: %02X %s\r\n", _providerName, _rp.operation, _rp.operation ? "On" : "Off");
#endif
        break;

    case 0x0020: // VOUT_SET 2 bytes Output voltage setting (format: value, F=0.01)
        _rp.outputVoltageSet = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s OutputVoltage(VOUT_SET): %.2fV\r\n", _providerName, _rp.outputVoltageSet);
#endif
        break;

    case 0x0030: // IOUT_SET 2 bytes Output current setting (format: value, F=0.01)
        _rp.outputCurrentSet = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s OutputCurrent(IOUT_SET): %.2fA\r\n", _providerName, _rp.outputCurrentSet);
#endif
        break;

    case 0x0040: // FAULT_STATUS  2 bytes Abnormal status
        _rp.FaultStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s FAULT_STATUS : %s : HI_TEMP: %d, OP_OFF: %d, AC_FAIL: %d, SHORT: %d, OLP: %d, OVP: %d, OTP: %d\r\n", _providerName,
                Word2BinaryString(_rp.FaultStatus),
                _rp.FAULT_STATUS.HI_TEMP,
                _rp.FAULT_STATUS.OP_OFF,
                _rp.FAULT_STATUS.AC_Fail,
                _rp.FAULT_STATUS.SHORT,
                _rp.FAULT_STATUS.OCP,
                _rp.FAULT_STATUS.OVP,
                _rp.FAULT_STATUS.OTP);
#endif
        break;

    case 0x0050: // READ_VIN	2 bytes Input voltage read value (format: value, F=0.1)
        _rp.inputVoltage = scaleValue(readUnsignedInt16(frame + 2), 0.1f);
        if (_model < NPB_Model_t::NPB_1200_24) _rp.inputVoltage = 230.0;
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s InputVoltage: %.1fV\r\n", _providerName, _rp.inputVoltage);
#endif
        break;

    case 0x0060: // READ_VOUT 2 bytes Output voltage read value (format: value, F=0.01)
        _rp.outputVoltage = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
        calcPower();
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s OutputVoltage: %.2fV\r\n", _providerName, _rp.outputVoltage);
#endif
        break;

    case 0x0061: // READ_IOUT 2 bytes Output current read value (format: value, F=0.01)
        _rp.outputCurrent = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
        calcPower();
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s OutputCurrent: %.2fA\r\n", _providerName, _rp.outputCurrent);
#endif
        break;

    case 0x0062: // READ_TEMPERATURE_1 2 bytes Internal ambient temperature (format: value, F=0.1)
        _rp.internalTemperature = scaleValue(readSignedInt16(frame + 2), 0.1f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Temperature: %.1f°C\r\n", _providerName, _rp.internalTemperature);
#endif
        break;

    case 0x0080: // MFR_ID_B0B5 6 bytes Manufacturer's name
        memcpy(reinterpret_cast<char*>(_rp.ManufacturerName), reinterpret_cast<char*>(frame + 2), 6);
        return;

    case 0x0081: // MFR_ID_B6B11 6 bytes Manufacturer's name
        strncpy(reinterpret_cast<char*>(&(_rp.ManufacturerName[6])), reinterpret_cast<char*>(frame + 2), 6);
        for (int i = 11; i > 0; i--) {
            if (isblank(_rp.ManufacturerName[i]))
                _rp.ManufacturerName[i] = 0;
            else
                break;
        }
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Manufacturer Name: '%s'\r\n", _providerName, _rp.ManufacturerName);
#endif
        break;

    case 0x0082: // MFR_MODEL_B0B51 6 bytes Manufacturer's model name
        memcpy(reinterpret_cast<char*>(_rp.ManufacturerModelName), reinterpret_cast<char*>(frame + 2), 6);
        return;

    case 0x0083: // MFR_MODEL_B6B11 6 bytes Manufacturer's model name
        {
        strncpy(reinterpret_cast<char*>(&(_rp.ManufacturerModelName[6])), reinterpret_cast<char*>(frame + 2), 6);
        for (int i = 11; i > 0; i--) {
            if (isblank(_rp.ManufacturerModelName[i]))
                _rp.ManufacturerModelName[i] = 0;
            else
                break;
        }

        auto& cMeanWell = Configuration.get().MeanWell;
        if (strcmp(_rp.ManufacturerModelName, "NPB-450-48") == 0) {
            _model = NPB_Model_t::NPB_450_48;
            cMeanWell.CurrentLimitMin = 1.36f; // 1.36A
            cMeanWell.CurrentLimitMax = 6.8f; // 6.8A
            cMeanWell.VoltageLimitMin = 42.0f;
            cMeanWell.VoltageLimitMax = 80.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-750-48") == 0) {
            _model = NPB_Model_t::NPB_750_48;
            cMeanWell.CurrentLimitMin = 2.26f; // 2.26A
            cMeanWell.CurrentLimitMax = 11.3f; // 11.3A
            cMeanWell.VoltageLimitMin = 42.0f;
            cMeanWell.VoltageLimitMax = 80.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1200-48") == 0) {
            _model = NPB_Model_t::NPB_1200_48;
            cMeanWell.CurrentLimitMin = 3.6f; // 3.6A
            cMeanWell.CurrentLimitMax = 18.0f; // 18.0A
            cMeanWell.VoltageLimitMin = 42.0f;
            cMeanWell.VoltageLimitMax = 80.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1700-48") == 0) {
            _model = NPB_Model_t::NPB_1700_48;
            cMeanWell.CurrentLimitMin = 5.0f; // 5.0A
            cMeanWell.CurrentLimitMax = 25.0f; // 25.0A
            cMeanWell.VoltageLimitMin = 42.0f;
            cMeanWell.VoltageLimitMax = 80.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-450-24") == 0) {
            _model = NPB_Model_t::NPB_450_24;
            cMeanWell.CurrentLimitMin = 2.7f; // 2.7A
            cMeanWell.CurrentLimitMax = 13.5f; // 13.5A
            cMeanWell.VoltageLimitMin = 21.0f;
            cMeanWell.VoltageLimitMax = 42.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-750-24") == 0) {
            _model = NPB_Model_t::NPB_750_24;
            cMeanWell.CurrentLimitMin = 4.5f; // 4.5A
            cMeanWell.CurrentLimitMax = 22.5f; // 22.5A
            cMeanWell.VoltageLimitMin = 21.0f;
            cMeanWell.VoltageLimitMax = 42.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1200-24") == 0) {
            _model = NPB_Model_t::NPB_1200_24;
            cMeanWell.CurrentLimitMin = 7.2f; // 7.2A
            cMeanWell.CurrentLimitMax = 36.0f; // 36.0A
            cMeanWell.VoltageLimitMin = 21.0f;
            cMeanWell.VoltageLimitMax = 42.0f;
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1700-24") == 0) {
            _model = NPB_Model_t::NPB_1700_24;
            cMeanWell.CurrentLimitMin = 10.0f; // 10.0A
            cMeanWell.CurrentLimitMax = 50.0f; // 50.0A
            cMeanWell.VoltageLimitMin = 21.0f;
            cMeanWell.VoltageLimitMax = 42.0f;
        } else {
            /* we didn't recognize the charge and set it to NPB-450-48 as default*/
            _model = NPB_Model_t::NPB_450_48;
            cMeanWell.CurrentLimitMin = 1.36f; // 1.36A
            cMeanWell.CurrentLimitMax = 6.8f; // 6.8A
            cMeanWell.VoltageLimitMin = 42.0f;
            cMeanWell.VoltageLimitMax = 80.0f;
        }
        // check if min/max current and voltage is in range of Meanwell charger (Limits)
        if (cMeanWell.MinCurrent < cMeanWell.CurrentLimitMin || cMeanWell.MinCurrent > cMeanWell.CurrentLimitMax) cMeanWell.MinCurrent = cMeanWell.CurrentLimitMin;
        if (cMeanWell.MaxCurrent < cMeanWell.CurrentLimitMin || cMeanWell.MaxCurrent > cMeanWell.CurrentLimitMax) cMeanWell.MaxCurrent = cMeanWell.CurrentLimitMax;
        if (cMeanWell.MinVoltage < cMeanWell.VoltageLimitMin || cMeanWell.MinVoltage > cMeanWell.VoltageLimitMax) cMeanWell.MinVoltage = cMeanWell.VoltageLimitMin;
        if (cMeanWell.MaxVoltage < cMeanWell.VoltageLimitMin || cMeanWell.MaxVoltage > cMeanWell.VoltageLimitMax) cMeanWell.MaxVoltage = cMeanWell.VoltageLimitMax;

//#ifdef MEANWELL_DEBUG_ENABLED
        //if (_verboseLogging)
            MessageOutput.printf("%s Manufacturer Model Name: '%s' %d\r\n", _providerName, _rp.ManufacturerModelName, static_cast<int>(_model));
//#endif
        }
        break;

    case 0x0084: // MFR_REVISION_B0B5 6 bytes Firmware revision
        memcpy(reinterpret_cast<char*>(_rp.FirmwareRevision), reinterpret_cast<char*>(frame + 2), 6);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) {
            char hex[13];
            hex[12] = 0;
            for (int i = 0; i < 6; i++) {
                uint8_t b = _rp.FirmwareRevision[i];
                uint8_t hn = (b >> 4) & 0x0F;
                uint8_t ln = b & 0x0F;
                hex[i * 2] = hn > 9 ? 'A' + hn - 10 : '0' + hn;
                hex[i * 2 + 1] = ln > 9 ? 'A' + ln - 10 : '0' + ln;
            }
            MessageOutput.printf("%s Firmware Revision: '%s'\r\n", _providerName, hex);
        }
#endif
        break;

    case 0x0085: // MFR_LOCATION_B0B2 3 bytes Manufacturer's factory location
        strncpy(reinterpret_cast<char*>(_rp.ManufacturerFactoryLocation), reinterpret_cast<char*>(frame + 2), 3);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Manufacturer Factory Location: '%s'\r\n", _providerName, _rp.ManufacturerFactoryLocation);
#endif
        break;

    case 0x0086: // MFR_DATE_B0B5 6 bytes Manufacturer date
        strncpy(reinterpret_cast<char*>(_rp.ManufacturerDate), reinterpret_cast<char*>(frame + 2), 6);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Manufacturer Date: '%s'\r\n", _providerName, _rp.ManufacturerDate);
#endif
        break;

    case 0x0087: // MFR_SERIAL_B0B5 6 bytes Product serial number
        memcpy(reinterpret_cast<char*>(_rp.ProductSerialNo), reinterpret_cast<char*>(frame + 2), 6);
        return;

    case 0x0088: // MFR_SERIAL_B6B11 6 bytes Product serial number
        strncpy(reinterpret_cast<char*>(&(_rp.ProductSerialNo[6])), reinterpret_cast<char*>(frame + 2), 6);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s Product Serial No '%s'\r\n", _providerName, _rp.ProductSerialNo);
#endif
        break;

    case 0x00B0: // CURVE_CC 2 bytes Constant current setting of charge curve (format: value, F=0.01)
        _rp.curveCC = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveCC: %.2fA\r\n", _providerName, _rp.curveCC);
#endif
        break;

    case 0x00B1: // CURVE_CV 2 bytes Constant voltage setting of charge curve (format: value, F=0.01)
        _rp.curveCV = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveCV: %.2fV\r\n", _providerName, _rp.curveCV);
#endif
        break;

    case 0x00B2: // CURVE_FV 2 bytes Floating voltage setting of charge curve (format: value, F=0.01)
        _rp.curveFV = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveFV: %.2fV\r\n", _providerName, _rp.curveFV);
#endif
        break;

    case 0x00B3: // CURVE_TC 2 bytes Taper current setting value of charging curve (format: value, F=0.01)
        _rp.curveTC = scaleValue(readUnsignedInt16(frame + 2), 0.01f);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveTC: %.2fA\r\n", _providerName, _rp.curveTC);
#endif
        break;

    case 0x00B4: // CURVE_CONFIG 2 bytes Configuration setting of charge curve
        _rp.CurveConfig = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CURVE_CONFIG : %s : CUVE: %d, STGS: %d, TCS: %d, CUVS: %X\r\n", _providerName,
                Word2BinaryString(_rp.CurveConfig),
                _rp.CURVE_CONFIG.CUVE,
                _rp.CURVE_CONFIG.STGS,
                _rp.CURVE_CONFIG.TCS,
                _rp.CURVE_CONFIG.CUVS);
#endif
        break;

    case 0x00B5: // CURVE_CC_TIMEOUT 2 bytes CC charge timeout setting of charging curve
        _rp.curveCC_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveCC_Timeout: %d minutes\r\n", _providerName, _rp.curveCC_Timeout);
#endif
        break;

    case 0x00B6: // CURVE_CV_TIMEOUT 2 bytes CV charge timeout setting of charging curve
        _rp.curveCV_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveCV_Timeout: %d minutes\r\n", _providerName, _rp.curveCV_Timeout);
#endif
        break;

    case 0x00B7: // CURVE_FV_TIMEOUT 2 bytes FV charge timeout setting of charging curve
        _rp.curveFV_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s CurveFV_Timeout: %d minutes\r\n", _providerName, _rp.curveFV_Timeout);
#endif
        break;

    case 0x00B8: // CHG_STATUS 2 bytes Charging status reporting
        _rp.ChargeStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CHG_STATUS : %s : BTNC: %d, WAKUP_STOP: %d, FVM: %d, CVM: %d, CCM: %d, FULLM: %d\r\n", _providerName,
                Word2BinaryString(_rp.ChargeStatus),
                _rp.CHG_STATUS.BTNC,
                _rp.CHG_STATUS.WAKEUP_STOP,
                _rp.CHG_STATUS.FVM,
                _rp.CHG_STATUS.CVM,
                _rp.CHG_STATUS.CCM,
                _rp.CHG_STATUS.FULLM);
#endif
        break;

    case 0x00B9: // CHG_RST_VBAT 2 bytes The voltage Rest to art the charging after the battery is fully
        {
            uint16_t ChgRstVbat = readUnsignedInt16(frame + 2);
            if (_verboseLogging) MessageOutput.printf("%s CHG_RST_VBAT: %d\r\n", _providerName, ChgRstVbat);
        }
        return;

    case 0x00C0: // SCALING_FACTOR 2 bytes Scaling ratio
        _rp.scalingFactor = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) MessageOutput.printf("%s ScalingFactor: %d, %04X\r\n", _providerName, _rp.scalingFactor, _rp.scalingFactor);
#endif
        break;

    case 0x00C1: // SYSTEM_STATUS 2 bytes System Status
        _rp.SystemStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s SYSTEM_STATUS : %s : EEPER: %d, INITIAL_STATE: %d, DC_OK: %d\r\n", _providerName,
                Word2BinaryString(_rp.SystemStatus),
                _rp.SYSTEM_STATUS.EEPER,
                _rp.SYSTEM_STATUS.INITIAL_STATE,
                _rp.SYSTEM_STATUS.DC_OK);
#endif
        break;

    case 0x00C2: // SYSTEM_CONFIG 2 bytes System Configuration
        _rp.SystemConfig = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging) {
            const char* OperationInit[4] = {
                "Power on with 00h: OFF",
                "Power on with 01h: ON, (default)",
                "Power on with the last setting",
                "No used"
            };
            MessageOutput.printf("%s SYSTEM_CONFIG : %s : Inital operational behavior: %s, EEPROM write disable: %d\r\n", _providerName,
                Word2BinaryString(_rp.SystemConfig),
                OperationInit[_rp.SYSTEM_CONFIG.OPERATION_INIT],
                _rp.SYSTEM_CONFIG.EEP_OFF);
        }
#endif
        break;

    default:;
        MessageOutput.printf("%s CAN: Unknown Command %04X, len %d\r\n", _providerName, readUnsignedInt16(frame), len);
        return;
    }

    _lastUpdate = millis();
}

bool MeanWellCanClass::updateAvailable(uint32_t since) const
{
    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
    return (_lastUpdate - since) < halfOfAllMillis;
}

void MeanWellCanClass::setupParameter()
{
    auto const& cMeanWell = Configuration.get().MeanWell;

    bool temp = _verboseLogging;
    _verboseLogging = true;

    if (_verboseLogging) MessageOutput.printf("%s read parameter\r\n", _providerName);

    // Switch Charger off
    _rp.operation = 0; // Operation OFF
    sendCmd(ChargerID, 0x0000, &_rp.operation, 1);
    vTaskDelay(100); // delay 100 tick
    getCanCharger();
    readCmd(ChargerID, 0x0000);

    readCmd(ChargerID, 0x0080); // read Manufacturer Name
    yield();
    readCmd(ChargerID, 0x0081); // read Manufacturer Name
    readCmd(ChargerID, 0x0082); // read Manufacturer Model Name
    yield();
    readCmd(ChargerID, 0x0083); // read Manufacturer Model Name
    readCmd(ChargerID, 0x0084); // read Firmware Revision
    yield();
    readCmd(ChargerID, 0x0085); // read Manufacturer Factory Location
    readCmd(ChargerID, 0x0086); // read Manufacture Date
    yield();
    readCmd(ChargerID, 0x0087); // read Product Serial No
    readCmd(ChargerID, 0x0088); // read Product Serial No
    yield();

    sendCmd(ChargerID, 0x0020, Float2Uint(53.0f / 0.01f), 2); // set Output Voltage
    vTaskDelay(100); // delay 100 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x0020); // read Output Voltage
    yield();

    sendCmd(ChargerID, 0x0030, Float2Uint(cMeanWell.MinCurrent / 0.01f), 2); // set Output Current
    vTaskDelay(100); // delay 100 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x0030); // read Output Current
    yield();

    sendCmd(ChargerID, 0x00B0, Float2Uint(cMeanWell.MaxCurrent / 0.01f), 2); // set Curve_CC
    vTaskDelay(100); // delay 100 tick
    yield();
    getCanCharger();
    yield();
    readCmd(ChargerID, 0x00B0); // read CURVE_CC
    yield();

    sendCmd(ChargerID, 0x00B1, Float2Uint(53.0f / 0.01f), 2); // set Curve_CV
    getCanCharger();
    readCmd(ChargerID, 0x00B1); // read CURVE_CV

    sendCmd(ChargerID, 0x00B2, Float2Uint(52.9f / 0.01f), 2); // set Curve_FV
    getCanCharger();
    readCmd(ChargerID, 0x00B2); // read CURVE_FV

    sendCmd(ChargerID, 0x00B3, Float2Uint(1.0f / 0.01f), 2); // set Curve_TC
    vTaskDelay(100); // delay 100 tick
    getCanCharger();
    readCmd(ChargerID, 0x00B3); // read CURVE_TC

    readCmd(ChargerID, 0x00B5); // read CURVE_CC_TIMEOUT
    yield();
    readCmd(ChargerID, 0x00B6); // read CURVE_CV_TIMEOUT
    yield();
    readCmd(ChargerID, 0x00B7); // read CURVE_FV_TIMEOUT
    yield();

    readCmd(ChargerID, 0x00B9); // read CHG_RST_VBAT
    yield();

    readCmd(ChargerID, 0x00C0); // read Scaling Factor
    yield();

    readCmd(ChargerID, 0x00C2); // read SYSTEM_CONFIG
    yield();
    _rp.SystemConfig = 0b0000000000000001; // Initial operation with power on 00: Power Supply is OFF
    _rp.SYSTEM_CONFIG.EEP_OFF = 1;  // disable realtime writing to EEPROM
    MessageOutput.printf("%s SystemConfig: %s\r\n", _providerName,  Word2BinaryString(_rp.SystemConfig));
    sendCmd(ChargerID, 0x00C2, reinterpret_cast<uint8_t*>(&_rp.SystemConfig), 2); // read SYSTEM_CONFIG
    vTaskDelay(100); // delay 200 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x00C2); // read SYSTEM_CONFIG
    yield();

    _rp.CurveConfig = 0; // first reset the configuration bits
    _rp.CURVE_CONFIG.CUVS = 0; // customized charging curve (default)
    _rp.CURVE_CONFIG.TCS = 1; // Temperature compensation -3mv/°C/cell (default)
    _rp.CURVE_CONFIG.STGS = 0; // 3 Stage charge mode
    _rp.CURVE_CONFIG.CUVE = 0; // Power supply mode
    sendCmd(ChargerID, 0x00B4, reinterpret_cast<uint8_t*>(&(_rp.CURVE_CONFIG)), 2);
    vTaskDelay(100); // delay 200 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x00B4); // read CURVE_CONFIG
    yield();

    readCmd(ChargerID, 0x0040); // read Fault Status
    yield();

    if (_verboseLogging) MessageOutput.printf("%s done\r\n", _providerName);

    _verboseLogging = temp;

    _setupParameter = false;
}

void MeanWellCanClass::updateEEPROMwrites2NVS() {

    static uint32_t lastupdated=millis();

    // test and update every 60 Minutes
    if (millis() - lastupdated > 60*1000*60) {
        if (EEPROMwrites != preferences.getULong(sEEPROMwrites)) {
            MessageOutput.printf("%s update EEPROMwrites=%u in NVS storage\r\n", _providerName, EEPROMwrites);
            preferences.putULong(sEEPROMwrites, EEPROMwrites);
        }
        lastupdated = millis();
    }
}

void MeanWellCanClass::loop()
{
    auto const& config = Configuration.get();

    if (!config.MeanWell.Enabled || !_initialized) {
        return;
    }

    if (_setupParameter) setupParameter();

    uint32_t t_start = millis();

    parseCanPackets();

    updateEEPROMwrites2NVS();

    if (t_start - _previousMillis < config.MeanWell.PollInterval * 1000) { return; }
    _previousMillis = t_start;

    static int state = 0;
    static constexpr uint16_t Cmnds[] = {
        0x0050,  // read VIN
        0x00C1,  // read SYSTEM_STATUS
        0x0062,  // read Temperature
        0x00B8,  // read CHARGE_STATUS
        0x0000,  // read ON/OFF Status
        0x0040   // read FAULT_STATUS
    };
    if (_verboseLogging) MessageOutput.printf("%s State: %d\r\n", _providerName, state);
    readCmd(ChargerID, Cmnds[state++]);
    if (state >= sizeof(Cmnds)/sizeof(uint16_t)) state = 0;

    readCmd(ChargerID, 0x0060); // read VOUT
    readCmd(ChargerID, 0x0061); // read IOUT

    float InverterPower = 0.0;
    String invName;
    String BattInvName;
    bool isProducing = false;
    bool isReachable = false;
    bool first = true;
    bool batteryConnected_isProducing = false;

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv != NULL) {
            if (inv->serial() != config.PowerLimiter.InverterId) {
                InverterPower += inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
                if (first) {
                    isProducing = inv->isProducing();
                    isReachable = inv->isReachable();
                    invName = inv->name();
                    first = false;
                } else {
                    isProducing = (isProducing && inv->isProducing()) ? true : false;
                    isReachable = (isReachable && inv->isReachable()) ? true : false;
                    invName += String(" + ") + inv->name();
                }
            } else {
                batteryConnected_isProducing = inv->isProducing();
                BattInvName = inv->name();
            }
        }
    }

    // static_cast<unsigned int>(inv0->Statistics()->getChannelFieldDigits(TYPE_AC, CH0, FLD_PAC)));
    float GridPower = PowerMeter.getPowerTotal();
    if (_verboseLogging)
        MessageOutput.printf("%s %lu ms, House Power: %.1fW, Grid Power: %.1fW, Inverter (%s) Day Power: %.1fW, Batt con. Inverter (%s), Charger Power: %.1fW\r\n", _providerName,
            millis() - t_start, PowerMeter.getHousePower(), GridPower, invName.c_str(), InverterPower, BattInvName.c_str(), _rp.outputPower);

    auto stats = Battery.getStats();

    if (!Battery.initialized()) goto exit;

    if (_automaticCharge) {
        if (_verboseLogging)
            MessageOutput.printf("%s automatic mode, it's %s, SOC: %.1f%%, %s%s%scharge%sabled, ChargeTemperatur is %svalid, %s is %sproducing, %s is %sproducing, Charger is %s", _providerName,
                SunPosition.isDayPeriod() ? "day" : "night",
                stats->getSoC(),
                stats->getAlarm().overVoltage ? "alarmOverVoltage, " : "",
                stats->getAlarm().underTemperature ? "alarmUnderTemperature, " : "",
                stats->getAlarm().overTemperature ? "alarmOverTemperature, " : "",
                stats->getChargeEnabled() ? "En" : "Dis",
                stats->isChargeTemperatureValid()? "" : "not ",
                invName.c_str(), isProducing ? "" : "not ",
                BattInvName.c_str(), batteryConnected_isProducing ? "" : "not ",
                _rp.operation ? "ON" : "OFF");

        boolean _fullChargeRequested = false;
        if (stats->getFullChargeRequest()) { _fullChargeRequested = true; }
        if (!stats->getChargeEnabled()) { _fullChargeRequested = false; }

        // check if battery overvoltage, or night, or inverter is not producing, than switch of charger
        if (stats->getAlarm().overVoltage ||
            stats->getAlarm().underTemperature ||
            stats->getAlarm().overTemperature ||
            !stats->isChargeTemperatureValid() ||
            !SunPosition.isDayPeriod() ||
            batteryConnected_isProducing ||
            !stats->getChargeEnabled() ||
            (!_fullChargeRequested &&
             stats->getSoC() >= config.Battery.Stop_Charging_BatterySoC_Threshold
            ) ||
            (!isProducing && config.MeanWell.mustInverterProduce) )
        {
            switchChargerOff("");

            // check if battery request immediate charging or SoC is less than Stop_Charging_BatterySoC_Threshold (20 ... 100%)
            // and inverter is producing and reachable and day
            // than switch on charger
        } else if ( (_fullChargeRequested ||
                     stats->getSoC() < config.Battery.Stop_Charging_BatterySoC_Threshold
                    ) &&
                    (
                      (isProducing &&
                       isReachable &&
                       config.MeanWell.mustInverterProduce
                      ) ||
                      !config.MeanWell.mustInverterProduce
                    ) &&
                    SunPosition.isDayPeriod()
                  )
        {
            if (!_rp.operation &&
                (GridPower < -config.MeanWell.MinCurrent * stats->getVoltage() ||
                 stats->getImmediateChargingRequest() ||
                 _fullChargeRequested
                )
               )
            {
                if (_verboseLogging) MessageOutput.println(", switch Charger ON");
                // only start if charger is off and enough GridPower is available
                setAutomaticChargeMode(true);
                setPower(true);
                float RecommendedChargeVoltageLimit = stats->getRecommendedChargeVoltageLimit();
                setValue(config.MeanWell.MinCurrent, MEANWELL_SET_CURRENT); // set minimum current to softstart charging
                setValue(config.MeanWell.MinCurrent, MEANWELL_SET_CURVE_CC); // set minimum current to softstart charging
                setValue(RecommendedChargeVoltageLimit - 0.25f, MEANWELL_SET_CURVE_CV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(RecommendedChargeVoltageLimit - 0.30f, MEANWELL_SET_CURVE_FV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(RecommendedChargeVoltageLimit - 0.25f, MEANWELL_SET_VOLTAGE); // set to battery recommended charge voltage according to BMS value and user manual
                readCmd(ChargerID, 0x0060); // read VOUT
                readCmd(ChargerID, 0x0061); // read IOUT
            } else {
                if (_verboseLogging) MessageOutput.println();
            }

            static boolean _chargeImmediateRequested = false;
            if (stats->getSoC() >= (config.Battery.Stop_Charging_BatterySoC_Threshold - 10) &&
                !_fullChargeRequested)
            {
                _chargeImmediateRequested = false;
            }

            if (_fullChargeRequested ||
                ((stats->getImmediateChargingRequest() || _chargeImmediateRequested) &&
                  stats->getSoC() < (config.Battery.Stop_Charging_BatterySoC_Threshold - 10)
                )
               )
            {
                if (_verboseLogging) MessageOutput.printf("%s Immediate Charge requested", _providerName);
                setValue(config.MeanWell.MaxCurrent, MEANWELL_SET_CURRENT);
                setValue(config.MeanWell.MaxCurrent, MEANWELL_SET_CURVE_CC);
                _chargeImmediateRequested = true;
            } else {
                // Zero Grid Export Charging Algorithm (Charger consums at operation minimum 180 Watt = 3.6A*50V)
                if (_verboseLogging) MessageOutput.printf("%s Zero Grid Charger controller", _providerName);
                float pCharger = config.MeanWell.MinCurrent * stats->getVoltage(); // Minimum power usage of charger
                float hysteresis = 25.0;
                float minPowerNeeded = pCharger;
                float efficiency = 95.0 / 100.0;
                if (_rp.inputPower > 10.0f) {
                    pCharger = _rp.inputPower;
                    minPowerNeeded = 0.0;
                    hysteresis = 0.0;
                    efficiency = _rp.efficiency / 100.0;
                }
                if ((GridPower < -(minPowerNeeded + hysteresis)) && ((fabs(GridPower) - minPowerNeeded) > config.MeanWell.Hysteresis)) { // 25 Watt Hysteresic
                    if (_verboseLogging) MessageOutput.printf(", increment");
                    // Solar Inverter produces enough power, we export to the Grid
                    if (_rp.outputCurrent >= config.MeanWell.MaxCurrent)
                    {
                        // do nothing, _OutputCurrentSetting && CurveCC is higher than actual OutputCurrent, the charger is regulating
                        if (_verboseLogging) MessageOutput.print(" not");
                    }
                    // check if outputCurrent is less than recommended charging current from battery
                    // if so than increase charging current by 0,5A (round about 25W)
                    else if (_rp.outputCurrent < stats->getRecommendedChargeCurrentLimit())
                    {
                        float increment = fabs(GridPower) / stats->getVoltage() * efficiency;
                        if (_verboseLogging) MessageOutput.printf(" by %.2f A", increment);
                        setValue(_rp.outputCurrentSet + increment, MEANWELL_SET_CURRENT);
                        setValue(_rp.outputCurrentSet, MEANWELL_SET_CURVE_CC);
                    }
                } else if ((GridPower >= 0.0) && (_rp.outputCurrent > 0.0f)) {
                    if (_verboseLogging) MessageOutput.printf(", decrement");
                    float decrement = GridPower / stats->getVoltage() * efficiency;
                    // check if Solar Inverter produces not enough power, then we have to reduce switch off the charger
                    // otherwise we have to reduce the OutputCurrent
                    if ((_rp.outputCurrent < config.MeanWell.MinCurrent - 0.01f &&
                         _rp.outputCurrentSet >= config.MeanWell.MinCurrent - 0.01f &&
                         _rp.outputCurrentSet <= config.MeanWell.MinCurrent + 0.024f
                        ) ||
                        (_rp.outputCurrentSet - decrement < config.MeanWell.MinCurrent)
                       )
                    {
                        // we have to switch off the charger, the charger consums with minimum OutputCurrent to much power.
                        // we can not reduce the power consumption and have to switch off the charger
                        // or battery is full and charger reduces output current via charge algorithm
                        // set current to minimum value
                        switchChargerOff(", not enough solar power");
                    }
                    // else if (_rp.OutputCurrent > _rp.CurveCC && _rp.OutputCurrent < _rp.OutputCurrentSetting) {
                    else if (_rp.outputCurrent > config.MeanWell.MinCurrent) {
                        if (_verboseLogging) MessageOutput.printf(" by %.2f A", decrement);
                        setValue(_rp.outputCurrentSet - decrement, MEANWELL_SET_CURRENT);
                        setValue(_rp.outputCurrentSet, MEANWELL_SET_CURVE_CC);
                    } else {
                        if (_verboseLogging) MessageOutput.printf(", sorry I don't know, OutputCurrent: %.3f, MinCurrent: %.3f",
                                                                    _rp.outputCurrent, config.MeanWell.MinCurrent);
                    }
                } else if (_rp.outputCurrent > 0.0f) {
                    if (_verboseLogging) MessageOutput.print(" constant");
                } else {
                    switchChargerOff(", unknown reason");
                }
                if (stats->getAlarm().overCurrentCharge || stats->getWarning().highCurrentCharge) {
                }
            }
        }

        if (_verboseLogging) MessageOutput.println();

    } else { // not automatic mode

        // Full charge or immediately charge requests handling in "non Automatic" mode
        static bool _FullChargeRequest = false;
        static bool _ChargeImmediately = false;
        if (stats->getFullChargeRequest()) _FullChargeRequest = true;
        if (stats->getImmediateChargingRequest()) _ChargeImmediately = true;
        if ( (stats->getFullChargeRequest() || _FullChargeRequest ||
              stats->getImmediateChargingRequest() || _ChargeImmediately
             ) &&
             !stats->getAlarm().overVoltage &&
             !stats->getAlarm().underTemperature &&
             !stats->getAlarm().overTemperature &&
             stats->isChargeTemperatureValid() &&
             SunPosition.isDayPeriod() &&
             stats->getChargeEnabled()
           )
        {
            if (!_rp.operation) {
                if (_verboseLogging) MessageOutput.printf("%s charge request\r\n", _FullChargeRequest?"Full":"Immediately");

                setPower(true);
                float RecommendedChargeVoltageLimit = stats->getRecommendedChargeVoltageLimit();
                setValue(config.MeanWell.MaxCurrent, MEANWELL_SET_CURRENT); // set maximum current to start charging
                setValue(config.MeanWell.MaxCurrent, MEANWELL_SET_CURVE_CC); // set maximum current to start charging
                setValue(RecommendedChargeVoltageLimit - 0.25f, MEANWELL_SET_CURVE_CV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(RecommendedChargeVoltageLimit - 0.30f, MEANWELL_SET_CURVE_FV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(RecommendedChargeVoltageLimit - 0.25f, MEANWELL_SET_VOLTAGE); // set to battery recommended charge voltage according to BMS value and user manual
            }

            if ( _ChargeImmediately &&
                !_FullChargeRequest &&
                stats->getSoC() >= config.Battery.Stop_Charging_BatterySoC_Threshold
               )
            {
                switchChargerOff("Battery immediately charge completed");
                _ChargeImmediately = false;
            }
            if (_FullChargeRequest && stats->getSoC() >= 99.9 )
            {
                switchChargerOff("Battery full charge completed");
                _FullChargeRequest = false;
            }
        }

        /*
                        if (_rp.Operation) {
                                if (GridPower < -(180-_rp.output_power)) {
                                        // charger is currently on and we export enough power to the Grid
                                        float chargeCurrent = -GridPower / Battery.voltage;
                                        if (chargeCurrent >= MeanWell_MinCurrent) { // Charger needs at minimum 3.6A OutputCurrent to work
                                                setValue(chargeCurrent, MEANWELL_SET_CURRENT);
                                        }
                                }
                        }
        */
    }

exit:;

    MessageOutput.printf("%s Round trip %lu ms\r\n", _providerName, millis() - t_start);
}

void MeanWellCanClass::switchChargerOff(const char* reason)
{
    if (_rp.operation) {
        if (_verboseLogging) MessageOutput.printf(", switch charger OFF%s", reason);
        setValue(0.0, MEANWELL_SET_CURRENT);
        setValue(0.0, MEANWELL_SET_CURVE_CC);
        setPower(false);
    } else {
        if (_rp.outputCurrent > 0.0) {
            if (_verboseLogging) MessageOutput.printf(", force charger to switch OFF%s", reason);
            setValue(0.0, MEANWELL_SET_CURRENT);
            setValue(0.0, MEANWELL_SET_CURVE_CC);
            setPower(false);
        } else {
            if (_verboseLogging) MessageOutput.printf(", charger is OFF%s", reason);
        }
    }
}

void MeanWellCanClass::setValue(float in, uint8_t parameterType)
{
    const char* type[] = { "Voltage", "Current", "Curve_CV", "Curve_CC", "Curve_FV", "Curve_TC" };
    const char unit[] = { 'V', 'A', 'V', 'A', 'V', 'A' };
    if (_verboseLogging)
        MessageOutput.printf("%s setValue %s: %.2f%c ... ", _providerName, type[parameterType], in, unit[parameterType]);

    auto const& cMeanWell = Configuration.get().MeanWell;
    auto stats = Battery.getStats();

    switch (parameterType) {
    case MEANWELL_SET_VOLTAGE:
        in = min(in, stats->getRecommendedChargeVoltageLimit()); // Pylontech US3000C max voltage limit
        in = max(in, stats->getRecommendedDischargeVoltageLimit()); // Pylontech US3000C min voltage limit

        readCmd(ChargerID, 0x0020); // read UOUT_SET
        if (fabs(_rp.outputVoltageSet - in) > 0.01) {
            sendCmd(ChargerID, 0x0020, Float2Uint(in / 0.01f), 2); // set Output Voltage
            vTaskDelay(100); // delay 100 tick
            getCanCharger();
            readCmd(ChargerID, 0x0020); // read UOUT_SET
        }
        break;

    case MEANWELL_SET_CURVE_CV:
        in = min(in, stats->getRecommendedChargeVoltageLimit()); // Pylontech US3000C max voltage limit
        in = max(in, stats->getRecommendedDischargeVoltageLimit()); // Pylontech US3000C min voltage limit

        readCmd(ChargerID, 0x00B1); // read Curve_CV
        if (fabs(_rp.curveCV - in) > 0.01) {
            sendCmd(ChargerID, 0x00B1, Float2Uint(in / 0.01f), 2); // set Curve_CV
            getCanCharger();
            readCmd(ChargerID, 0x00B1); // read Curve_CV
        }
        break;

    case MEANWELL_SET_CURVE_FV:
        in = min(in, stats->getRecommendedChargeVoltageLimit()); // Pylontech US3000C max voltage limit
        in = min(in, _rp.curveCV); // Pylontech US3000C max voltage limit, must be below or equal constant voltage curveCV
        in = max(in, stats->getRecommendedDischargeVoltageLimit()); // Pylontech US3000C min voltage limit

        readCmd(ChargerID, 0x00B2); // read Curve_FV
        if (fabs(_rp.curveFV - in) > 0.01) {
            sendCmd(ChargerID, 0x00B2, Float2Uint(in / 0.01f), 2); // set Curve_FV
            getCanCharger();
            readCmd(ChargerID, 0x00B2); // read Curve_FV
        }
        break;

    case MEANWELL_SET_CURRENT:
        in = min(in, cMeanWell.MaxCurrent); // Meanwell NPB-xxxx-xx OutputCurrent max limit
        in = max(in, cMeanWell.MinCurrent); // Meanwell NPB-xxxx-xx OutputCurrent min limit

        readCmd(ChargerID, 0x0030); // read IOUT_SET
        if (fabs(_rp.outputCurrentSet - in) > 0.01) {
            sendCmd(ChargerID, 0x0030, Float2Uint(in / 0.01f), 2); // set Output Current
            vTaskDelay(100); // delay 100 tick
            getCanCharger();
            readCmd(ChargerID, 0x0030); // read IOUT_SET
        }
        break;

    case MEANWELL_SET_CURVE_CC:
        in = min(in, cMeanWell.MaxCurrent); // Meanwell NPB-xxxx-xx OutputCurrent max limit
        in = max(in, cMeanWell.MinCurrent); // Meanwell NPB-xxxx-xx OutputCurrent min limit

        readCmd(ChargerID, 0x00B0); // read Curve_CC
        if (fabs(_rp.curveCC -in) > 0.01) {
            sendCmd(ChargerID, 0x00B0, Float2Uint(in / 0.01f), 2); // set Curve_CC
            vTaskDelay(100); // delay 100 tick
            getCanCharger();
            readCmd(ChargerID, 0x00B0); // read Curve_CC
        }
        break;

    case MEANWELL_SET_CURVE_TC:
        in = min(in, cMeanWell.MaxCurrent / 3.333333333f); // 3.3% Meanwell NPB-xxxx-xx OutputCurrent max limit
        in = max(in, cMeanWell.MinCurrent / 10.0f); // 10% of Meanwell NPB-xxxx-xx OutputCurrent min limit

        readCmd(ChargerID, 0x00B3); // read Curve_TC
        if (fabs(_rp.curveTC) > 0.01) {
            sendCmd(ChargerID, 0x00B3, Float2Uint(in / 0.01f), 2); // set Curve_TC
            getCanCharger();
            readCmd(ChargerID, 0x00B3); // read Curve_TC
        }
        break;
    default:;
        return;
    }
    yield();

    if (_verboseLogging)
        MessageOutput.println(" done");
}

void MeanWellCanClass::setPower(bool power)
{
    if (_verboseLogging)
        MessageOutput.printf("%s setPower %s\r\n", _providerName, power ? "on" : "off");

    /*
        if (_meanwell_power > 0) {
            digitalWrite(_meanwell_power, !power);
        }
    */
    _rp.CURVE_CONFIG.CUVE = (power == true) ? 1 : 0; // enable/disable automatic charger
    sendCmd(ChargerID, 0x00B4, reinterpret_cast<uint8_t*>(&(_rp.CURVE_CONFIG)), 2);
    vTaskDelay(100); // delay 100 tick
    getCanCharger();
    readCmd(ChargerID, 0x00B4); // read CURVE_CONFIG
    yield();

    // Switch Charger on/off
    _rp.operation = (power == true) ? 1 : 0;
    sendCmd(ChargerID, 0x0000, &_rp.operation, 1);
    vTaskDelay(100); // delay 100 tick
    getCanCharger();
    readCmd(ChargerID, 0x0000);

    if (power)
        _lastPowerCommandSuccess = (_rp.operation == 1) && (_rp.CURVE_CONFIG.CUVE == 1) ? true : false;
    else
        _lastPowerCommandSuccess = (_rp.operation == 0) && (_rp.CURVE_CONFIG.CUVE == 0) ? true : false;

    yield();

    if (_verboseLogging)
        MessageOutput.println(" done");
}

const char* MeanWellCanClass::Word2BinaryString(uint16_t w)
{
    static char binary[18];
    for (int i = 0; i < 8; i++) {
        binary[i] = ((w >> (15 - i)) & 1) + '0';
    }
    binary[8] = ' ';
    for (int i = 0; i < 8; i++) {
        binary[i + 9] = ((w >> (7 - i)) & 1) + '0';
    }
    binary[17] = 0;
    return binary;
}

uint8_t* MeanWellCanClass::Float2Uint(float value)
{
    static uint8_t rc[2];
    uint16_t t = (uint16_t)value;
    rc[0] = t & 0xFF;
    rc[1] = (t >> 8);
    return rc;
}

uint8_t* MeanWellCanClass::Bool2Byte(bool value)
{
    static uint8_t rc;
    rc = value == true ? 255 : 0;
    return &rc;
}

uint16_t MeanWellCanClass::readUnsignedInt16(uint8_t* data)
{
    uint8_t bytes[2];
    bytes[0] = *data;
    bytes[1] = *(data + 1);
    return (bytes[1] << 8) + bytes[0];
}

volatile uint8_t semaphore = 0;

bool MeanWellCanClass::sendCmd(uint8_t id, uint16_t cmd, uint8_t* data, int len)
{

    while (semaphore)
        yield();
    semaphore++;
    bool rc = _sendCmd(id, cmd, data, len);
    semaphore--;
    return rc;
}

bool MeanWellCanClass::_sendCmd(uint8_t id, uint16_t cmd, uint8_t* data, int len)
{
    twai_message_t tx_message = {};
    memset(tx_message.data, 0, sizeof(tx_message.data));
    tx_message.data[0] = (uint8_t)cmd;
    tx_message.data[1] = (uint8_t)(cmd >> 8);
    if (len > 0 && data != reinterpret_cast<uint8_t*>(NULL)) {
        memcpy(&tx_message.data[2], data, len);
    }
    tx_message.extd = 1;
    tx_message.data_length_code = len + 2;
    tx_message.identifier = 0x000C0100 | id;

    auto _provider = PinMapping.get().charger.provider;

    if (_verboseLogging) {
        char data[32];
        int i, j=0;
        for (i=0; i<tx_message.data_length_code; i++) {
            if (i>0)data[j++] = ' ';
            char c = tx_message.data[i]>>4;
            c += c<=9?'0':'A'-10;
            data[j++] = c;
            c = tx_message.data[i]&0xF;
            c += c<=9?'0':'A'-10;
            data[j++] = c;
        }
        data[j]=0;
        MessageOutput.printf("%s: id: %08X extd: %d len: %d data:[%s]\r\n", _providerName,
            tx_message.identifier, tx_message.extd, tx_message.data_length_code, data);
    }

    yield();
    uint32_t packetMarginTime = millis() - _meanwellLastResponseTime;
    if (packetMarginTime < 5)
    vTaskDelay(5 - packetMarginTime); // ensure minimum packet time  of 5ms between last response and new request

    switch (_provider) {
#ifdef USE_CHARGER_CAN0
        case Charger_Provider_t::CAN0:
            // Queue message for transmission

            if (twai_transmit(&tx_message, pdMS_TO_TICKS(1000)) == ESP_OK) {
                yield();
#ifdef MEANWELL_DEBUG_ENABLED__
                if (_verboseLogging)
                    MessageOutput.printf("%s Message queued for transmission cmnd %04X with %d data bytes\r\n", _providerName, cmd, len + 2);
#endif
                if (len>0) EEPROMwrites++;

            } else {
                yield();
                MessageOutput.printf("%s Failed to queue message for transmission\r\n", _providerName);
                return false;
            }
            break;
#endif
#ifdef USE_CHARGER_I2C
        case Charger_Provider_t::I2C0:
        case Charger_Provider_t::I2C1:
            i2c_can->sendMsgBuf(tx_message.identifier, (uint8_t)tx_message.extd, tx_message.data_length_code, tx_message.data);
            break;
#endif
#ifdef USE_CHARGER_MCP2515
        case Charger_Provider_t::MCP2515:
            {
            uint8_t sndStat;
            if ((sndStat = CAN->sendMsgBuf(reinterpret_cast<can_message_t*>(&tx_message))) != CAN_OK) {
                MessageOutput.printf("%s Error Sending Message... Status: %d\r\n", _providerName, sndStat);
            // } else {
                // MessageOutput.printf("%s Message Sent Successfully!\r\n", _providerName);
            }
            }
            break;
#endif
        default:;
    }

    yield();

    return true;
}

bool MeanWellCanClass::readCmd(uint8_t id, uint16_t cmd)
{
    bool rc = false;

    while (semaphore)
        yield();
    semaphore++;
    if (semaphore > 1)
        MessageOutput.printf("%s Semaphore = %d\r\n", _providerName, semaphore);

    if (_sendCmd(id, cmd) == false) { // read parameter
        semaphore--;
        return rc;
    }
    uint32_t t = millis();
    while (millis() - t < 2000) {
        if ((rc = getCanCharger()) == true)
            break;
        yield();
    }

    semaphore--;

    return rc;
}

template <typename T>
void addInputValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["inputValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addOutputValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["outputValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void MeanWellCanClass::generateJsonResponse(JsonVariant& root)
{
    root["data_age"] = (millis() - getLastUpdate()) / 1000;
    root["manufacturerModelName"] = String(_rp.ManufacturerName) + " " + String(_rp.ManufacturerModelName);
    root["automatic"] = _automaticCharge;
    addInputValue(root, "inputVoltage", _rp.inputVoltage, "V", 1);
    addInputValue(root, "inputPower", _rp.inputPower, "W", 1);
    addInputValue(root, "efficiency", _rp.efficiency, "%", 1);
    addInputValue(root, "internalTemperature", _rp.internalTemperature, "°C", 1);
    root["operation"] = _rp.operation ? true : false;
    root["stgs"] = _rp.CURVE_CONFIG.STGS ? true : false;
    root["cuve"] = _rp.CURVE_CONFIG.CUVE ? true : false;
    addInputValue(root, sEEPROMwrites, EEPROMwrites, "", 0);
    addOutputValue(root, "outputVoltage", _rp.outputVoltage, "V", 2);
    addOutputValue(root, "outputCurrent", _rp.outputCurrent, "A", 2);
    addOutputValue(root, "outputPower", _rp.outputPower, "W", 1);
    addOutputValue(root, "outputVoltageSet", _rp.outputVoltageSet, "V", 2);
    addOutputValue(root, "outputCurrentSet", _rp.outputCurrentSet, "A", 2);
    addOutputValue(root, "curveCV", _rp.curveCV, "V", 2);
    addOutputValue(root, "curveCC", _rp.curveCC, "A", 2);
    addOutputValue(root, "curveFV", _rp.curveFV, "V", 2);
    addOutputValue(root, "curveTC", _rp.curveTC, "A", 2);
    /*
            if (_verboseLogging) {
                String output;
            serializeJson(root, output);
            MessageOutput.println(output);
            }
    */
}

#endif
