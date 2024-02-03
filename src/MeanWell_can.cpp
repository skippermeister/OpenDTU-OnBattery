// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 - 2024 Andreas Högner
 */
#ifndef CHARGER_HUAWEI

#define MEANWELL_DEBUG_ENABLED
// #define MEANWELL_DEBUG_ENABLED__

#include <Arduino.h>
#include "MeanWell_can.h"
#include "MessageOutput.h"
#include "Battery.h"
#include "SunPosition.h"
#include "PowerMeter.h"
#include "PinMapping.h"
#include "Configuration.h"
#include <Hoymiles.h>
#ifdef CHARGER_USE_CAN0
#include <driver/twai.h>
#else
#include <SPI.h>
#include <mcp_can.h>
#endif
#include <math.h>
#include <AsyncJson.h>

static constexpr char TAG[] = "[MEANWELL]";

MeanWellCanClass MeanWellCan;

SemaphoreHandle_t xSemaphore = NULL;

#ifdef CHARGER_USE_CAN0

MeanWellCanClass::MeanWellCanClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&MeanWellCanClass::loop, this))
{
}

bool MeanWellCanClass::enable(void)
{
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        MessageOutput.print("Twai driver installed");
    } else {
        MessageOutput.println("ERR:Failed to install Twai driver.");
        return false;
    }

    // Start TWAI driver
    if (twai_start() == ESP_OK) {
        MessageOutput.print(" and started. ");
    } else {
        MessageOutput.println(". ERR:Failed to start Twai driver.");
        return false;
    }
    return true;
}
#endif

void MeanWellCanClass::init(Scheduler& scheduler)
{
    _previousMillis = millis();

    if (xSemaphore == NULL)
        xSemaphore = xSemaphoreCreateMutex();

    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;
    if (!cMeanWell.Enabled)
        return;

    MessageOutput.print("Initialize MeanWell AC charger interface... ");

    if (!PinMapping.isValidMeanWellConfig()) {
        MessageOutput.println("Invalid pin config");
        return;
    }

    if (!_initialized) {
        const PinMapping_t& pin = PinMapping.get();
#ifdef CHARGER_USE_CAN0
        auto tx = static_cast<gpio_num_t>(pin.can0_tx);
        auto rx = static_cast<gpio_num_t>(pin.can0_rx);

        MessageOutput.printf("CAN0 port rx = %d, tx = %d. ", rx, tx);

        g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);
        //    	g_config.bus_off_io = (gpio_num_t)can0_stb;
        g_config.bus_off_io = (gpio_num_t)-1;
        if (!enable())
            return;
#else
        MessageOutput.printf("MCP2515 CAN miso = %d, mosi = %d, clk = %d, irq = %d, cs = %d, power_pin = %d. ", pin.mcp2515_miso, pin.mcp2515_mosi, pin.mcp2515_clk, pin.mcp2515_irq, pin.mcp2515_cs;

	    spi = new SPIClass(HSPI);
    	spi->begin(pin.mcp2515_clk, pin.mcp2515_miso, pin.mcp2515_mosi, pin.mcp2515_cs);
	    pinMode(pin.mcp2515_cs, OUTPUT);
    	digitalWrite(pin.mcp2515_cs, HIGH);

	    pinMode(pin.mcp2515_irq, INPUT_PULLUP);
    	_mcp2515_irq = pin.mcp2515_irq;

	    auto mcp_frequency = MCP_8MHZ;
    	if (16000000UL == frequency) {
            mcp_frequency = MCP_16MHZ; }
    	else if (8000000UL != frequency) {
            MessageOutput.printf("%s CAN: unknown frequency %d Hz, using 8 MHz\r\n", TAG, mcp_frequency);
    	}
	    CAN = new MCP_CAN(spi, mcp2515_cs);
	    if (!CAN->begin(MCP_ANY, CAN_125KBPS, mcp_frequency) == CAN_OK) {
            MessageOutput.println("Error Initializing MCP2515...");
            return;
	    }

	    // Change to normal mode to allow messages to be transmitted
    	CAN->setMode(MCP_NORMAL);
#endif

        scheduler.addTask(_loopTask);
        _loopTask.enable();

        MessageOutput.print("Initialized Successfully! ");

        _initialized = true;
    }

    MessageOutput.println("done");
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
        MessageOutput.printf("%s xSemapore not initialized\r\n", TAG);
        return false;
    }

    /* See if we can obtain the semaphore.  If the semaphore is not
       available wait 1000 ticks to see if it becomes free. */
    if (xSemaphoreTake(xSemaphore, (TickType_t)1000) == pdTRUE) {
#ifdef CHARGER_USE_CAN0
        // Check for messages. twai_recive is blocking when there is no data so we return if there are no frames in the buffer
        twai_status_info_t status_info;
        if (twai_get_status_info(&status_info) != ESP_OK) {
            MessageOutput.printf("%s Failed to get Twai status info\r\n", TAG);
            xSemaphoreGive(xSemaphore);
            return false;
        }
        if (status_info.msgs_to_rx == 0) {
            //  MessageOutput.printf("%s no message received\r\n", TAG);
            xSemaphoreGive(xSemaphore);
            return false;
        }

        // Wait for message to be received, function is blocking
        twai_message_t rx_message;
        if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
            MessageOutput.printf("%s Failed to receive message\r\n", TAG);
            xSemaphoreGive(xSemaphore);
            return false;
        }
        _meanwellLastResponseTime = millis(); // save last response time
        yield();

#ifdef MEANWELL_DEBUG_ENABLED__
        if (_verboseLogging)
            MessageOutput.printf("%s id: 0x%08X, data len: %d bytes\r\n", TAG, rx_message.identifier, rx_message.data_length_code);
#endif
        switch (rx_message.identifier & 0xFFFFFF00) {
        case 0x000C0100:
            // MessageOutput.printf("%s 0x%08X len: %d\r\n", TAG, rx_message.identifier, rx_message.data_length_code);
            xSemaphoreGive(xSemaphore);
            return false;
        case 0x000C0000:
            onReceive(rx_message.data, rx_message.data_length_code);
        }
#else
        uint32_t rxId;
        unsigned char len = 0;
        unsigned char rxBuf[8];

        if (digitalRead(_mcp2515_irq)) {
            //  MessageOutput.printf("%s no message received\r\n", TAG);
            xSemaphoreGive(xSemaphore);
            return false;
        }

        // If CAN_INT pin is low, read receive buffer
        CAN->readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

        // Determine if ID is standard (11 bits) or extended (29 bits)
        // MessageOutput.printf("%s Extended ID: 0x%.8lX  DLC: %1d\r\n", TAG, (rxId & 0x1FFFFFFF), len);
        //  MessageOutput.printf("%s id: 0x%08X, data len: %d bytes\r\n", TAG, rx_message.identifier, rx_message.data_length_code);
        switch (rxId & 0xFFFFFF00) {
        case 0x000C0100:
            // MessageOutput.printf("%s 0x%08X len: %d\r\n", TAG, rx_message.identifier, rx_message.data_length_code);
            xSemaphoreGive(xSemaphore);
            return false;
        case 0x000C0000:
            onReceive(rxBuf, len);
        }
#endif
        xSemaphoreGive(xSemaphore);
        return true;
    }

    MessageOutput.printf("%s xSemapore not free\r\n", TAG);

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
        { 0.0, 0.750 },
        { 100.0, 0.9000 },
        { 177.0, 0.9222 },
        { 222.0, 0.9535 },
        { 440.0, 0.9522 },
        { 666.0, 0.9498 },
        { 888.0, 0.9380 },
        { 1000.0, 0.9250 },
        { 1100.0, 0.9200 },
        { 1300.0, 0.9150 }
    };

    float scaling = 1.0;
    switch (_model) {
    case NPB_Model_t::NPB_450_48:
        scaling = 450.0 / 1200.0;
        break;
    case NPB_Model_t::NPB_750_48:
        scaling = 750.0 / 1200.0;
        break;
    case NPB_Model_t::NPB_1200_48:
        scaling = 1200.0 / 1200.0;
        break;
    case NPB_Model_t::NPB_1700_48:
        scaling = 1700.0 / 1200.0;
        break;
    case NPB_Model_t::NPB_unknown:
        scaling = 1.0;
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

void MeanWellCanClass::onReceive(uint8_t* frame, uint8_t len)
{
    switch (readUnsignedInt16(frame)) { // parse Command
    case 0x0000: // OPERATION 1 byte ON/OFF control
        _rp.operation = *(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Operation: %02X %s\r\n", TAG, _rp.operation, _rp.operation ? "On" : "Off");
#endif
        _lastUpdate = millis();
        break;

    case 0x0020: // VOUT_SET 2 bytes Output voltage setting (format: value, F=0.01)
        _rp.outputVoltageSet = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s OutputVoltage(VOUT_SET): %.2fV\r\n", TAG, _rp.outputVoltageSet);
#endif
        _lastUpdate = millis();
        break;

    case 0x0030: // IOUT_SET 2 bytes Output current setting (format: value, F=0.01)
        _rp.outputCurrentSet = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s OutputCurrent(IOUT_SET): %.2fA\r\n", TAG, _rp.outputCurrentSet);
#endif
        _lastUpdate = millis();
        break;

    case 0x0040: // FAULT_STATUS  2 bytes Abnormal status
        _rp.FaultStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s FAULT_STATUS : %s : HI_TEMP: %d, OP_OFF: %d, AC_FAIL: %d, SHORT: %d, OLP: %d, OVP: %d, OTP: %d\r\n", TAG,
                Word2BinaryString(_rp.FaultStatus),
                _rp.FAULT_STATUS.HI_TEMP,
                _rp.FAULT_STATUS.OP_OFF,
                _rp.FAULT_STATUS.AC_Fail,
                _rp.FAULT_STATUS.SHORT,
                _rp.FAULT_STATUS.OCP,
                _rp.FAULT_STATUS.OVP,
                _rp.FAULT_STATUS.OTP);
#endif
        _lastUpdate = millis();
        break;

    case 0x0050: // READ_VIN	2 bytes Input voltage read value (format: value, F=0.1)
        _rp.inputVoltage = scaleValue(readUnsignedInt16(frame + 2), 0.1);
        if (_model == NPB_Model_t::NPB_450_48)
            _rp.inputVoltage = 230.0;
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s InputVoltage: %.1fV\r\n", TAG, _rp.inputVoltage);
#endif
        _lastUpdate = millis();
        break;

    case 0x0060: // READ_VOUT 2 bytes Output voltage read value (format: value, F=0.01)
        _rp.outputVoltage = scaleValue(readUnsignedInt16(frame + 2), 0.01);
        _rp.outputPower = _rp.outputCurrent * _rp.outputVoltage;
        _rp.inputPower = _rp.outputPower / calcEfficency(_rp.outputPower) + (0.75 * 240.0 / 1000.0); // efficiency of NPB-1200-48 and leakage
        _rp.efficiency = (_rp.inputPower > 0.0) ? 100.0 * _rp.outputPower / _rp.inputPower : 0.0;
        _lastUpdate = millis();
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s OutputVoltage: %.2fV\r\n", TAG, _rp.outputVoltage);
#endif
        break;
    case 0x0061: // READ_IOUT 2 bytes Output current read value (format: value, F=0.01)
        _rp.outputCurrent = scaleValue(readUnsignedInt16(frame + 2), 0.01);
        _rp.outputPower = _rp.outputCurrent * _rp.outputVoltage;
        _rp.inputPower = _rp.outputPower / calcEfficency(_rp.outputPower) + (0.75 * 240.0 / 1000.0); // efficiency of NPB-1200-48 and leakage
        _rp.efficiency = (_rp.inputPower > 0.0) ? 100.0 * _rp.outputPower / _rp.inputPower : 0.0;
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s OutputCurrent: %.2fA\r\n", TAG, _rp.outputCurrent);
#endif
        _lastUpdate = millis();
        break;

    case 0x0062: // READ_TEMPERATURE_1 2 bytes Internal ambient temperature (format: value, F=0.1)
        _rp.internalTemperature = scaleValue(readSignedInt16(frame + 2), 0.1);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Temperature: %.1f°C\r\n", TAG, _rp.internalTemperature);
#endif
        _lastUpdate = millis();
        break;

    case 0x0080: // MFR_ID_B0B5 6 bytes Manufacturer's name
        memcpy(reinterpret_cast<char*>(_rp.ManufacturerName), reinterpret_cast<char*>(frame + 2), 6);
        break;

    case 0x0081: // MFR_ID_B6B11 6 bytes Manufacturer's name
        strncpy(reinterpret_cast<char*>(&(_rp.ManufacturerName[6])), reinterpret_cast<char*>(frame + 2), 6);
        for (int i = 11; i > 0; i--) {
            if (isblank(_rp.ManufacturerName[i]))
                _rp.ManufacturerName[i] = 0;
            else
                break;
        }
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Manufacturer Name: '%s'\r\n", TAG, _rp.ManufacturerName);
#endif
        _lastUpdate = millis();
        break;

    case 0x0082: // MFR_MODEL_B0B51 6 bytes Manufacturer's model name
        memcpy(reinterpret_cast<char*>(_rp.ManufacturerModelName), reinterpret_cast<char*>(frame + 2), 6);
        break;

    case 0x0083: // MFR_MODEL_B6B11 6 bytes Manufacturer's model name
    {
        strncpy(reinterpret_cast<char*>(&(_rp.ManufacturerModelName[6])), reinterpret_cast<char*>(frame + 2), 6);
        for (int i = 11; i > 0; i--) {
            if (isblank(_rp.ManufacturerModelName[i]))
                _rp.ManufacturerModelName[i] = 0;
            else
                break;
        }

        MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;
        if (strcmp(_rp.ManufacturerModelName, "NPB-450-48") == 0) {
            _model = NPB_Model_t::NPB_450_48;
            cMeanWell.MinCurrent = 1.36; // 1.36A
            cMeanWell.MaxCurrent = 6.8; // 6.8A
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-750-48") == 0) {
            _model = NPB_Model_t::NPB_750_48;
            cMeanWell.MinCurrent = 2.26; // 2.26A
            cMeanWell.MaxCurrent = 11.3; // 11.3A
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1200-48") == 0) {
            _model = NPB_Model_t::NPB_1200_48;
            cMeanWell.MinCurrent = 3.6; // 3.6A
            cMeanWell.MaxCurrent = 18.0; // 18.0A
        } else if (strcmp(_rp.ManufacturerModelName, "NPB-1700-48") == 0) {
            _model = NPB_Model_t::NPB_1700_48;
            cMeanWell.MinCurrent = 5.0; // 5.0A
            cMeanWell.MaxCurrent = 25.0; // 25.0A
        } else {
            _model = NPB_Model_t::NPB_450_48;
            cMeanWell.MinCurrent = 1.36; // 1.36A
            cMeanWell.MaxCurrent = 6.8; // 6.8A
        }

#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Manufacturer Model Name: '%s' %d\r\n", TAG, _rp.ManufacturerModelName, static_cast<int>(_model));
#endif
        _lastUpdate = millis();
    } break;

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
            MessageOutput.printf("%s Firmware Revision: '%s'\r\n", TAG, hex);
        }
#endif
        _lastUpdate = millis();
        break;

    case 0x0085: // MFR_LOCATION_B0B2 3 bytes Manufacturer's factory location
        strncpy(reinterpret_cast<char*>(_rp.ManufacturerFactoryLocation), reinterpret_cast<char*>(frame + 2), 3);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Manufacturer Factory Location: '%s'\r\n", TAG, _rp.ManufacturerFactoryLocation);
#endif
        _lastUpdate = millis();
        break;

    case 0x0086: // MFR_DATE_B0B5 6 bytes Manufacturer date
        strncpy((char*)_rp.ManufacturerDate, (char*)frame + 2, 6);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Manufacturer Date: '%s'\r\n", TAG, _rp.ManufacturerDate);
#endif
        _lastUpdate = millis();
        break;

    case 0x0087: // MFR_SERIAL_B0B5 6 bytes Product serial number
        memcpy(reinterpret_cast<char*>(_rp.ProductSerialNo), reinterpret_cast<char*>(frame + 2), 6);
        break;

    case 0x0088: // MFR_SERIAL_B6B11 6 bytes Product serial number
        strncpy(reinterpret_cast<char*>(&(_rp.ProductSerialNo[6])), reinterpret_cast<char*>(frame + 2), 6);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s Product Serial No '%s'\r\n", TAG, _rp.ProductSerialNo);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B0: // CURVE_CC 2 bytes Constant current setting of charge curve (format: value, F=0.01)
        _rp.curveCC = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveCC: %.2fA\r\n", TAG, _rp.curveCC);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B1: // CURVE_CV 2 bytes Constant voltage setting of charge curve (format: value, F=0.01)
        _rp.curveCV = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveCV: %.2fV\r\n", TAG, _rp.curveCV);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B2: // CURVE_FV 2 bytes Floating voltage setting of charge curve (format: value, F=0.01)
        _rp.curveFV = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveFV: %.2fV\r\n", TAG, _rp.curveFV);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B3: // CURVE_TC 2 bytes Taper current setting value of charging curve (format: value, F=0.01)
        _rp.curveTC = scaleValue(readUnsignedInt16(frame + 2), 0.01);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveTC: %.2f\r\n", TAG, _rp.curveTC);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B4: // CURVE_CONFIG 2 bytes Configuration setting of charge curve
        _rp.CurveConfig = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CURVE_CONFIG : %s : CUVE: %d, STGS: %d, TCS: %d, CUVS: %X\r\n", TAG,
                Word2BinaryString(_rp.CurveConfig),
                _rp.CURVE_CONFIG.CUVE,
                _rp.CURVE_CONFIG.STGS,
                _rp.CURVE_CONFIG.TCS,
                _rp.CURVE_CONFIG.CUVS);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B5: // CURVE_CC_TIMEOUT 2 bytes CC charge timeout setting of charging curve
        _rp.curveCC_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveCC_Timeout: %d minutes\r\n", TAG, _rp.curveCC_Timeout);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B6: // CURVE_CV_TIMEOUT 2 bytes CV charge timeout setting of charging curve
        _rp.curveCV_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveCV_Timeout: %d minutes\r\n", TAG, _rp.curveCV_Timeout);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B7: // CURVE_FV_TIMEOUT 2 bytes FV charge timeout setting of charging curve
        _rp.curveFV_Timeout = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CurveFV_Timeout: %d minutes\r\n", TAG, _rp.curveFV_Timeout);
#endif
        _lastUpdate = millis();
        break;

    case 0x00B8: // CHG_STATUS 2 bytes Charging status reporting
        _rp.ChargeStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s CHG_STATUS : %s : BTNC: %d, WAKUP_STOP: %d, FVM: %d, CVM: %d, CCM: %d, FULLM: %d\r\n", TAG,
                Word2BinaryString(_rp.ChargeStatus),
                _rp.CHG_STATUS.BTNC,
                _rp.CHG_STATUS.WAKEUP_STOP,
                _rp.CHG_STATUS.FVM,
                _rp.CHG_STATUS.CVM,
                _rp.CHG_STATUS.CCM,
                _rp.CHG_STATUS.FULLM);
#endif
        _lastUpdate = millis();
        break;

    case 0x00C0: // SCALING_FACTOR 2 bytes Scaling ratio
        _rp.scalingFactor = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s ScalingFactor: %f\r\n", TAG, _rp.scalingFactor);
#endif
        _lastUpdate = millis();
        break;

    case 0x00C1: // SYSTEM_STATUS 2 bytes System Status
        _rp.SystemStatus = readUnsignedInt16(frame + 2);
#ifdef MEANWELL_DEBUG_ENABLED
        if (_verboseLogging)
            MessageOutput.printf("%s SYSTEM_STATUS : %s : EEPER: %d, INITIAL_STATE: %d, DC_OK: %d\r\n", TAG,
                Word2BinaryString(_rp.SystemStatus),
                _rp.SYSTEM_STATUS.EEPER,
                _rp.SYSTEM_STATUS.INITIAL_STATE,
                _rp.SYSTEM_STATUS.DC_OK);
#endif
        _lastUpdate = millis();
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
            MessageOutput.printf("%s SYSTEM_CONFIG : %s : Inital operational behavior: %s\r\n", TAG,
                Word2BinaryString(_rp.SystemConfig),
                OperationInit[_rp.SYSTEM_CONFIG.OPERATION_INIT]);
        }
#endif
        _lastUpdate = millis();
        break;

    default:;
        MessageOutput.printf("%s CAN: Unknown Command %04X, len %d\r\n", TAG, readUnsignedInt16(frame), len);
    }
}

void MeanWellCanClass::setupParameter()
{
    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;

    if (_verboseLogging)
        MessageOutput.printf("%s read parameter\r\n", TAG);
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

    sendCmd(ChargerID, 0x0020, Float2Uint(53.0 / 0.01), 2); // set Output Voltage
    vTaskDelay(100); // delay 100 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x0020); // read Output Voltage
    yield();

    sendCmd(ChargerID, 0x0030, Float2Uint(cMeanWell.MinCurrent / 0.01), 2); // set Output Current
    vTaskDelay(100); // delay 100 tick
    yield();
    getCanCharger();
    readCmd(ChargerID, 0x0030); // read Output Current
    yield();

    sendCmd(ChargerID, 0x00B0, Float2Uint(cMeanWell.MaxCurrent / 0.01), 2); // set Curve_CC
    vTaskDelay(100); // delay 200 tick
    yield();
    getCanCharger();
    yield();
    readCmd(ChargerID, 0x00B0); // read CURVE_CC
    yield();

    sendCmd(ChargerID, 0x00B1, Float2Uint(53.0 / 0.01), 2); // set Curve_CV
    getCanCharger();
    readCmd(ChargerID, 0x00B1); // read CURVE_CV

    sendCmd(ChargerID, 0x00B2, Float2Uint(52.9 / 0.01), 2); // set Curve_FV
    getCanCharger();
    readCmd(ChargerID, 0x00B2); // read CURVE_FV

    sendCmd(ChargerID, 0x00B3, Float2Uint(1.0 / 0.01), 2); // set Curve_TC
    vTaskDelay(100); // delay 100 tick
    getCanCharger();
    readCmd(ChargerID, 0x00B3); // read CURVE_TC

    readCmd(ChargerID, 0x00B5); // read CURVE_CC_TIMEOUT
    yield();
    readCmd(ChargerID, 0x00B6); // read CURVE_CV_TIMEOUT
    yield();
    readCmd(ChargerID, 0x00B7); // read CURVE_FV_TIMEOUT
    yield();

    readCmd(ChargerID, 0x00C0); // read Scaling Factor
    yield();

    _rp.SystemConfig = 0b0000000000000000; // Initial operation with power on 00: Power Supply is OFF
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

    if (_verboseLogging)
        MessageOutput.printf("%s done\r\n", TAG);

    _setupParameter = false;
}

void MeanWellCanClass::loop()
{
    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;

    if (!cMeanWell.Enabled || !_initialized) {
        _setupParameter = true;
        return;
    }

    if (_setupParameter)
        setupParameter();

    uint32_t t_start = millis();

    parseCanPackets();

    if (t_start - _previousMillis < cMeanWell.PollInterval * 1000) {
        return;
    }
    _previousMillis = t_start;

    static int state = 0;
    switch (state) {
    case 0:
        readCmd(ChargerID, 0x0050); // read VIN
        break;

    case 1:
        readCmd(ChargerID, 0x00C1); // read SYSTEM_STATUS
        break;

    case 2:
        readCmd(ChargerID, 0x0062); // read Temperature
        break;

    case 3:
        readCmd(ChargerID, 0x00B8); // read CHARGE_STATUS
        break;

    case 4:
        readCmd(ChargerID, 0x0000); // read ON/OFF Status
        break;

    case 5:
        readCmd(ChargerID, 0x0040); // read FAULT_STATUS
        break;
    }
    readCmd(ChargerID, 0x0060); // read VOUT
    readCmd(ChargerID, 0x0061); // read IOUT
    if (++state == 6)
        state = 0;

    float InverterPower = 0.0;
    char invName[MAX_NAME_LENGTH + (INV_MAX_COUNT - 1) * (MAX_NAME_LENGTH + 3)] = "";
    bool isProducing = false;
    bool isReachable = false;
    bool first = true;

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv != NULL) {
            if (i != Configuration.get().PowerLimiter.InverterId) {
                InverterPower += inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);
                if (first) {
                    isProducing = inv->isProducing();
                    isReachable = inv->isReachable();
                    strcpy(invName, inv->name());
                    first = false;
                } else {
                    isProducing = (isProducing && inv->isProducing()) ? true : false;
                    isReachable = (isReachable && inv->isReachable()) ? true : false;
                    strcat(invName, " + ");
                    strcat(invName, inv->name());
                }
            }
        }
    }

    // static_cast<unsigned int>(inv0->Statistics()->getChannelFieldDigits(TYPE_AC, CH0, FLD_PAC)));
    float GridPower = PowerMeter.getPowerTotal(false);
    if (_verboseLogging)
        MessageOutput.printf("%s State: %d, %lu ms, House Power: %.1fW, Grid Power: %.1fW, Inverter (%s) Day Power: %.1fW, Charger Power: %.1fW\r\n", TAG,
            state, millis() - t_start, PowerMeter.getHousePower(), GridPower, invName, InverterPower, _rp.outputPower);

    if (!Battery.getStats()->initialized())
        goto exit;

    if (_automaticCharge) {
        if (_verboseLogging)
            MessageOutput.printf("%s automatic mode, it's %s, SOC: %d%%, %scharge%sabled, %s is %sproducing, Charger is %s", TAG,
                SunPosition.isDayPeriod() ? "day" : "night",
                Battery.getStats()->getSoC(),
                Battery.getStats()->getAlarm().overVoltage ? "alarmOverVoltage " : "",
                Battery.getStats()->getChargeEnabled() ? "En" : "Dis",
                invName,
                isProducing ? "" : "not ",
                _rp.operation ? "ON" : "OFF");

        // check if battery overvoltage, or night, or inverter is not producing, than switch of charger
        if (Battery.getStats()->getAlarm().overVoltage
            || Battery.getStats()->getAlarm().underTemperature
            || Battery.getStats()->getAlarm().overTemperature
            || !Battery.getStats()->isChargeTemperatureValid()
            || !SunPosition.isDayPeriod()
            || !Battery.getStats()->getChargeEnabled()
            || !isProducing) {
            switchChargerOff("");

            // check if battery request immediate charging or SoC is less than 100%
            // and inverter is producing and reachable and day
            // than switch on charger
        } else if (Battery.getStats()->getSoC() < 100 && isProducing && isReachable && SunPosition.isDayPeriod() /*&& Battery.chargeEnabled*/) {
            if (!_rp.operation && (GridPower < -cMeanWell.MinCurrent * Battery.getStats()->getVoltage() || Battery.getStats()->getChargeImmediately())) {
                if (_verboseLogging)
                    MessageOutput.println(", switch Charger ON");
                // only start if charger is off and enough GridPower is available
                setAutomaticChargeMode(true);
                setPower(true);
                setValue(cMeanWell.MinCurrent, MEANWELL_SET_CURRENT); // set minimum current to softstart charging
                setValue(cMeanWell.MinCurrent, MEANWELL_SET_CURVE_CC); // set minimum current to softstart charging
                setValue(Battery.getStats()->getRecommendedChargeVoltageLimit() - 0.25, MEANWELL_SET_CURVE_CV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(Battery.getStats()->getRecommendedChargeVoltageLimit() - 0.30, MEANWELL_SET_CURVE_FV); // set to battery recommended charge voltage according to BMS value and user manual
                setValue(Battery.getStats()->getRecommendedChargeVoltageLimit() - 0.25, MEANWELL_SET_VOLTAGE); // set to battery recommended charge voltage according to BMS value and user manual
                readCmd(ChargerID, 0x0060); // read VOUT
                readCmd(ChargerID, 0x0061); // read IOUT
            } else {
                if (_verboseLogging)
                    MessageOutput.println();
            }

            static boolean _chargeImmediateRequested = false;
            if (Battery.getStats()->getSoC() >= (Configuration.get().PowerLimiter.BatterySocStartThreshold - 10)) {
                _chargeImmediateRequested = false;
            }

            if ((Battery.getStats()->getChargeImmediately() || _chargeImmediateRequested) && Battery.getStats()->getSoC() < (Configuration.get().PowerLimiter.BatterySocStartThreshold - 10)) {
                if (_verboseLogging)
                    MessageOutput.printf("%s Immediate Charge requested", TAG);
                setValue(cMeanWell.MaxCurrent, MEANWELL_SET_CURRENT);
                setValue(cMeanWell.MaxCurrent, MEANWELL_SET_CURVE_CC);
                _chargeImmediateRequested = true;
            } else {
                // Zero Grid Export Charging Algorithm (Charger consums at operation minimum 180 Watt = 3.6A*50V)
                if (_verboseLogging)
                    MessageOutput.printf("%s Zero Grid Charger controller", TAG);
                float pCharger = _rp.outputCurrentSet * Battery.getStats()->getVoltage();
                if (_rp.outputPower > 0.0)
                    pCharger = _rp.outputPower;
                if (GridPower - _rp.outputPower < -(pCharger + cMeanWell.Hysteresis)) { // 25 Watt Hysteresic
                    if (_verboseLogging)
                        MessageOutput.printf(", increment");
                    // Solar Inverter produces enough power, we export to the Grid
                    //					if (_rp.OutputCurrent < _rp.CurveCC || _rp.OutputCurrent < _rp.OutputCurrentSetting) {
                    if (_rp.outputCurrent >= cMeanWell.MaxCurrent) {
                        // do nothing, _OutputCurrentSetting && CurveCC is higher than actual OutputCurrent, the charger is regulating
                        if (_verboseLogging)
                            MessageOutput.print(" not");
                    }
                    // check if outputCurrent is less than recommended charging current from battery
                    // if so than increase charging current by 0,5A (round about 25W)
                    else if (_rp.outputCurrent < Battery.getStats()->getRecommendedChargeCurrentLimit()) {
                        float increment = fabs(GridPower) / Battery.getStats()->getVoltage();
                        if (_verboseLogging)
                            MessageOutput.printf(" by %.2f A", increment);
                        setValue(_rp.outputCurrentSet + increment, MEANWELL_SET_CURRENT);
                        setValue(_rp.outputCurrentSet, MEANWELL_SET_CURVE_CC);
                    }
                } else if (GridPower - _rp.outputPower > -pCharger && _rp.outputCurrent > 0.0) {
                    if (_verboseLogging)
                        MessageOutput.printf(", decrement");
                    float decrement = fabs(GridPower) / Battery.getStats()->getVoltage();
                    // check if Solar Inverter produces not enough power, then we have to reduce switch off the charger
                    // otherwise we have to reduce the OutputCurrent
                    if ((_rp.outputCurrent < cMeanWell.MinCurrent - 0.01
                            && _rp.outputCurrentSet >= cMeanWell.MinCurrent - 0.01
                            && _rp.outputCurrentSet <= cMeanWell.MinCurrent + 0.024)
                        || (_rp.outputCurrentSet - decrement < cMeanWell.MinCurrent)) {
                        // we have to switch off the charger, the charger consums with minimum OutputCurrent to much power.
                        // we can not reduce the power consumption and have to switch off the charger
                        // or battery is full and charger reduces output current via charge algorithm
                        // set current to minimum value
                        switchChargerOff(", not enough solar power");
                    }
                    //					else if (_rp.OutputCurrent > _rp.CurveCC && _rp.OutputCurrent < _rp.OutputCurrentSetting) {
                    else if (_rp.outputCurrent > cMeanWell.MinCurrent) {
                        if (_verboseLogging)
                            MessageOutput.printf(" by %.2f A", decrement);
                        setValue(_rp.outputCurrentSet - decrement, MEANWELL_SET_CURRENT);
                        setValue(_rp.outputCurrentSet, MEANWELL_SET_CURVE_CC);
                    } else {
                        if (_verboseLogging)
                            MessageOutput.printf(", sorry I don't know, OutputCurrent: %.3f, MinCurrent: %.3f", _rp.outputCurrent, cMeanWell.MinCurrent);
                    }
                } else if (_rp.outputCurrent > 0.0) {
                    if (_verboseLogging)
                        MessageOutput.print(" constant");
                } else {
                    switchChargerOff(", unknown reason");
                }
                if (Battery.getStats()->getAlarm().overCurrentCharge || Battery.getStats()->getWarning().highCurrentCharge) {
                }
            }
        }

        if (_verboseLogging)
            MessageOutput.println();

    } else { // not automatic mode
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

    MessageOutput.printf("%s Round trip %lu ms\r\n", TAG, millis() - t_start);
}

void MeanWellCanClass::switchChargerOff(const char* reason)
{
    if (_rp.operation) {
        if (_verboseLogging)
            MessageOutput.printf(", switch charger OFF%s", reason);
        setValue(0.0, MEANWELL_SET_CURRENT);
        setValue(0.0, MEANWELL_SET_CURVE_CC);
        setPower(false);
    } else {
        if (_rp.outputCurrent > 0.0) {
            if (_verboseLogging)
                MessageOutput.printf(", force charger to switch OFF%s", reason);
            setValue(0.0, MEANWELL_SET_CURRENT);
            setValue(0.0, MEANWELL_SET_CURVE_CC);
            setPower(false);
        } else {
            if (_verboseLogging)
                MessageOutput.printf(", charger is OFF%s", reason);
        }
    }
}

void MeanWellCanClass::setValue(float in, uint8_t parameterType)
{
    const char* type[] = { "Voltage", "Current", "Curve_CV", "Curve_CC", "Curve_FV", "Curve_TC" };
    const char unit[] = { 'V', 'A', 'V', 'A', 'V', 'A' };
    if (_verboseLogging)
        MessageOutput.printf("%s setValue %s: %.2f%c ... ", TAG, type[parameterType], in, unit[parameterType]);

    const MeanWell_CONFIG_T& cMeanWell = Configuration.get().MeanWell;

    switch (parameterType) {
    case MEANWELL_SET_VOLTAGE:
        if (in > cMeanWell.MaxVoltage)
            in = cMeanWell.MaxVoltage; // Pylontech US3000C max voltage limit
        if (in < cMeanWell.MinVoltage)
            in = cMeanWell.MinVoltage; // Pylontech US3000C min voltage limit

        sendCmd(ChargerID, 0x0020, Float2Uint(in / 0.01), 2); // set Output Voltage
        vTaskDelay(100); // delay 100 tick
        getCanCharger();
        readCmd(ChargerID, 0x0020); // read UOUT_SET
        break;

    case MEANWELL_SET_CURVE_CV:
        if (in > cMeanWell.MaxVoltage)
            in = cMeanWell.MaxVoltage; // Pylontech US3000C max voltage limit
        if (in < cMeanWell.MinVoltage)
            in = cMeanWell.MinVoltage; // Pylontech US3000C min voltage limit
        sendCmd(ChargerID, 0x00B1, Float2Uint(in / 0.01), 2); // set Curve_CV
        getCanCharger();
        readCmd(ChargerID, 0x00B1); // read Curve_CV
        break;

    case MEANWELL_SET_CURVE_FV:
        if (in > cMeanWell.MaxVoltage)
            in = cMeanWell.MaxVoltage; // Pylontech US3000C max voltage limit
        if (in > _rp.curveCV)
            in = _rp.curveCV; // Pylontech US3000C max voltage limit
        if (in < cMeanWell.MinVoltage)
            in = cMeanWell.MinVoltage; // Pylontech US3000C min voltage limit
        sendCmd(ChargerID, 0x00B2, Float2Uint(in / 0.01), 2); // set Curve_FV
        getCanCharger();
        readCmd(ChargerID, 0x00B2); // read Curve_FV
        break;

    case MEANWELL_SET_CURRENT:
        if (in > cMeanWell.MaxCurrent)
            in = cMeanWell.MaxCurrent; // Meanwell NPB-1200-48 OutputCurrent max limit
        if (in < cMeanWell.MinCurrent)
            in = cMeanWell.MinCurrent; // Meanwell NPB-1200-48 OutputCurrent min limit

        sendCmd(ChargerID, 0x0030, Float2Uint(in / 0.01), 2); // set Output Current
        vTaskDelay(100); // delay 100 tick
        getCanCharger();
        readCmd(ChargerID, 0x0030); // read IOUT_SET
        break;

    case MEANWELL_SET_CURVE_CC:
        if (in > cMeanWell.MaxCurrent)
            in = cMeanWell.MaxCurrent; // Meanwell NPB-1200-48 OutputCurrent max limit
        if (in < cMeanWell.MinCurrent)
            in = cMeanWell.MinCurrent; // Meanwell NPB-1200-48 OutputCurrent min limit

        sendCmd(ChargerID, 0x00B0, Float2Uint(in / 0.01), 2); // set Curve_CC
        vTaskDelay(100); // delay 100 tick
        getCanCharger();
        readCmd(ChargerID, 0x00B0); // read Curve_CC
        break;

    case MEANWELL_SET_CURVE_TC:
        if (in > cMeanWell.MaxCurrent / 3.333333333)
            in = cMeanWell.MaxCurrent / 3.333333333; // Meanwell NPB-1200-48 OutputCurrent max limit
        if (in < cMeanWell.MinCurrent / 10.0)
            in = cMeanWell.MinCurrent / 10.0; // Meanwell NPB-1200-48 OutputCurrent min limit
        sendCmd(ChargerID, 0x00B3, Float2Uint(in / 0.01), 2); // set Curve_TC
        getCanCharger();
        readCmd(ChargerID, 0x00B3); // read Curve_TC
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
        MessageOutput.printf("%s setPower %s\r\n", TAG, power ? "on" : "off");

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
#ifdef CHARGER_USE_CAN0
    twai_message_t tx_message;
    memset(tx_message.data, 0, sizeof(tx_message.data));
    tx_message.data[0] = (uint8_t)cmd;
    tx_message.data[1] = (uint8_t)(cmd >> 8);
#ifdef MEANWELL_DEBUG_ENABLED__
    MessageOutput.printf("%s %d %04X %d", TAG, id, cmd, len);
#endif
    if (len > 0 && data != reinterpret_cast<uint8_t*>(NULL)) {
#ifdef MEANWELL_DEBUG_ENABLED__
        MessageOutput.print(" [");
#endif
        for (int i = 0; i < len; i++) {
            tx_message.data[2 + i] = data[i];
#ifdef MEANWELL_DEBUG_ENABLED__
            MessageOutput.printf("%02X ", data[i]);
#endif
        }
#ifdef MEANWELL_DEBUG_ENABLED__
        MessageOutput.print("]");
#endif
    }
#ifdef MEANWELL_DEBUG_ENABLED__
    MessageOutput.println();
#endif
    tx_message.extd = 1;
    tx_message.data_length_code = len + 2;
    tx_message.identifier = 0x000C0100 | id;
#ifdef MEANWELL_DEBUG_ENABLED__
    MessageOutput.printf("%s %d %04X %d\r\n", TAG, id, cmd, len);
#endif
    // Queue message for transmission

    yield();
    uint32_t packetMarginTime = millis() - _meanwellLastResponseTime;
    if (packetMarginTime < 5)
        vTaskDelay(5 - packetMarginTime); // ensure minimum packet time  of 5ms between last response and new request
    if (twai_transmit(&tx_message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        yield();
#ifdef MEANWELL_DEBUG_ENABLED__
        if (_verboseLogging)
            MessageOutput.printf("%s Message queued for transmission cmnd %04X with %d data bytes\r\n", TAG, cmd, len + 2);
#endif
    } else {
        yield();
        MessageOutput.printf("%s Failed to queue message for transmission\r\n", TAG);
        return false;
    }
#else
    char tx_message[8];
    memset(tx_message, 0, sizeof(tx_message));
    tx_message[0] = (uint8_t)cmd;
    tx_message[1] = (uint8_t)(cmd >> 8);
    //    MessageOutput.printf("%s %d %04X %d ", TAG, id, cmd, len);
    if (len > 0 && data != reinterpret_cast<uint8_t*>(NULL)) {
        //		MessageOutput.print(" [");
        for (int i = 0; i < len; i++) {
            tx_message[2 + i] = data[i];
            //			MessageOutput.printf("%02X ", data[i]);
        }
        //		MessageOutput.print("]");
    }
    // MessageOutput.println();
    uint8_t sndStat = CAN->sendMsgBuf(0x000C0100 | id, 1, len + 2, tx_message);
    if (sndStat == CAN_OK) {
        //        MessageOutput.printf("%s Message Sent Successfully!\r\n", TAG);
    } else {
        MessageOutput.printf("%s Error Sending Message... Status: %d\r\n", TAG, sndStat);
    }
#endif
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
        MessageOutput.printf("%s Semaphore = %d\r\n", TAG, semaphore);

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
