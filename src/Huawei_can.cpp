// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#ifdef USE_CHARGER_HUAWEI

#include "Battery.h"
#include "Huawei_can.h"
#include "MessageOutput.h"
#include "PowerMeter.h"
#include "PowerLimiter.h"
#include "Configuration.h"
#include <SPI.h>
#include <mcp_can.h>
#include "SPIPortManager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>

HuaweiCanClass HuaweiCan;
HuaweiCanCommClass HuaweiCanComm;

static constexpr char TAG[] = "[HuaweiCanClass]";
// *******************************************************
// Huawei CAN Communication
// *******************************************************

// Using a C function to avoid static C++ member
void HuaweiCanCommunicationTask(void* parameter) {
  for( ;; ) {
    HuaweiCanComm.loop();
    yield();
  }
}

bool HuaweiCanCommClass::init() {

    if (!PinMapping.isValidChargerConfig()) {
        MessageOutput.println("Invalid pin config");
        return false;
    }

    auto const& pin = PinMapping.get().charger;
    switch (pin.provider) {
#ifdef USE_CHARGER_CAN0
        case Charger_Provider_t::CAN0:
            {
            auto tx = static_cast<gpio_num_t>(pin.can0.tx);
            auto rx = static_cast<gpio_num_t>(pin.can0.rx);

            MessageOutput.printf("CAN0 port rx = %d, tx = %d\r\n", rx, tx);

            twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);
            // g_config.bus_off_io = (gpio_num_t)can0_stb;
#if defined(BOARD_HAS_PSRAM)
            g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;
#endif
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
            }
            break;
#endif
#ifdef USE_CHARGER_I2C
        case Charger_Provider_t::I2C0:
        case Charger_Provider_t::I2C1:
            {
            auto scl = pin.i2c.scl;
            auto sda = pin.i2c.sda;

            MessageOutput.printf("I2C CAN Bus @ I2C%d scl = %d, sda = %d\r\n", pin.provider==Charger_Provider_t::I2C0?0:1, scl, sda);

            i2c_can = new I2C_CAN(pin.provider == Charger_Provider_t::I2C0?&Wire:&Wire1, 0x25, scl, sda, 400000UL);     // Set I2C Address

            int i = 10;
            while (CAN_OK != i2c_can->begin(I2C_CAN_125KBPS))    // init can bus : baudrate = 125k
            {
                delay(200);
                if (--i) continue;

                MessageOutput.println("CAN Bus FAIL!");
                return false;
            }

            const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
            const uint32_t myFilter = 0x1081407F;       // filter for this message only
            i2c_can->init_Mask(0, 1, myMask);
            i2c_can->init_Filt(0, 1, myFilter);
            i2c_can->init_Mask(1, 1, myMask);

            MessageOutput.println("I2C CAN Bus OK!");
            }
            break;
#endif
#ifdef USE_CHARGER_MCP2515
        case Charger_Provider_t::MCP2515:
            {
            auto oSPInum = SPIPortManager.allocatePort("MCP2515");
            if (!oSPInum) { return false; }

            SPI = new SPIClass(*oSPInum); // old value HSPI
            SPI->begin(pin.mcp2515.clk, pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.cs);
            pinMode(pin.mcp2515.cs, OUTPUT);
            digitalWrite(pin.mcp2515.cs, HIGH);

            _mcp2515Irq = pin.mcp2515.irq;
            pinMode(_mcp2515Irq, INPUT_PULLUP);

            auto frequency = Configuration.get().MCP2515.Controller_Frequency;
            auto mcp_frequency = MCP_8MHZ;
            if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
            else if (8000000UL != frequency) {
                MessageOutput.printf("Huawei CAN: unknown frequency %d Hz, using 8 MHz\r\n", mcp_frequency);
            }

            _CAN = new MCP_CAN(SPI, pin.mcp2515.cs);
            if (!_CAN->begin(MCP_STDEXT, CAN_125KBPS, mcp_frequency) == CAN_OK) {
                return false;
            }

            const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
            const uint32_t myFilter = 0x1081407F;       // filter for this message only
            _CAN->init_Mask(0, 1, myMask);
            _CAN->init_Filt(0, 1, myFilter);
            _CAN->init_Mask(1, 1, myMask);

            // Change to normal mode to allow messages to be transmitted
            _CAN->setMode(MCP_NORMAL);
            }
            break;
#endif
        default:;
            MessageOutput.println(" Error: no IO provider configured");
            return false;
    }

    return true;
}

// Public methods need to obtain semaphore

void HuaweiCanCommClass::loop()
{
  std::lock_guard<std::mutex> lock(_mutex);

  twai_message_t rx_message;
  uint8_t i;
  bool gotMessage = false;
  auto _provider = PinMapping.get().charger.provider;

  switch (_provider) {
#ifdef USE_CHARGER_MCP2515
    case Charger_Provider_t::MCP2515:
        if (digitalRead(_mcp2515Irq)) break;

        // If CAN_INT pin is low, read receive buffer
        _CAN->readMsgBuf(&rx_message.identifier, &rx_message.data_length_code, rx_message.data); // Read data: len = data length, buf = data byte(s)
        gotMessage = true;
        break;
#endif
#ifdef USE_CHARGER_CAN0
    case Charger_Provider_t::CAN0:
        {
        // Check for messages. twai_recive is blocking when there is no data so we return if there are no frames in the buffer
        twai_status_info_t status_info;
        if (twai_get_status_info(&status_info) != ESP_OK) {
            MessageOutput.printf("%s Failed to get Twai status info\r\n", TAG);
            break;
        }

        if (status_info.msgs_to_rx) {
            // Wait for message to be received, function is blocking
            if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) == ESP_OK) {
                gotMessage = true;
            } else {
                MessageOutput.printf("%s Failed to receive message\r\n", TAG);
            }
        } else {
            //  MessageOutput.printf("%s no message received\r\n", TAG);
        }
        }
        break;
#endif
#ifdef USE_CHARGER_I2C
    case Charger_Provider_t::I2C0:
    case Charger_Provider_t::I2C1:
        if (CAN_MSGAVAIL != i2c_can->checkReceive()) break;

        if (CAN_OK == i2c_can->readMsgBuf(&rx_message.data_length_code, rx_message.data)) {    // read data,  len: data length, buf: data buf
            if (rx_message.data_length_code > 0 && rx_message.data_length_code <= 8) {
                rx_message.identifier = i2c_can->getCanId();
                rx_message.extd = i2c_can->isExtendedFrame();
                rx_message.rtr = i2c_can->isRemoteRequest();
                gotMessage = true;
            } else {
                MessageOutput.printf("I2C CAN received %d bytes\r\n", rx_message.data_length_code);
            }
        } else {
            MessageOutput.println("I2C CAN nothing received");
        }
        break;
#endif
    default:;
  }

  if (gotMessage) {
    if((rx_message.identifier & 0x80000000) == 0x80000000) {   // Determine if ID is standard (11 bits) or extended (29 bits)
      if ((rx_message.identifier & 0x1FFFFFFF) == 0x1081407F && rx_message.data_length_code == 8) {

        uint32_t value = __bswap32(* reinterpret_cast<uint32_t*> (rx_message.data + 4));
        uint8_t msgId = rx_message.data[1];

        // Input power 0x70, Input frequency 0x71, Input current 0x72
        // Output power 0x73, Efficiency 0x74, Output Voltage 0x75 and Output Current 0x76
        if(msgId >= 0x70 && msgId <= 0x76 ) {
          _recValues[msgId - 0x70] = value;
        }

        // Input voltage
        else if(msgId == 0x78 ) {
          _recValues[HUAWEI_INPUT_VOLTAGE_IDX] = value;
        }

        // Output Temperature
        else if(msgId == 0x7F ) {
          _recValues[HUAWEI_OUTPUT_TEMPERATURE_IDX] = value;
        }

        // Input Temperature 0x80, Output Current 1 0x81 and Output Current 2 0x82
        else if(msgId >= 0x80 && msgId <= 0x82 ) {
          _recValues[msgId - 0x80 + HUAWEI_INPUT_TEMPERATURE_IDX] = value;
        }

        // This is the last value that is send
        if(msgId == 0x81) {
          _completeUpdateReceived = true;
        }
      }
    }
    // Other emitted codes not handled here are: 0x1081407E (Ack), 0x1081807E (Ack Frame), 0x1081D27F (Description), 0x1001117E (Whr meter), 0x100011FE (unclear), 0x108111FE (output enabled), 0x108081FE (unclear). See:
    // https://github.com/craigpeacock/Huawei_R4850G2_CAN/blob/main/r4850.c
    // https://www.beyondlogic.org/review-huawei-r4850g2-power-supply-53-5vdc-3kw/
  }

  // Transmit values
  for (i = 0; i < HUAWEI_OFFLINE_CURRENT; i++) {
    if ( _hasNewTxValue[i] == true) {
      uint8_t data[8] = {0x01, i, 0x00, 0x00, 0x00, 0x00, (uint8_t)((_txValues[i] & 0xFF00) >> 8), (uint8_t)(_txValues[i] & 0xFF)};

      // Send extended message
      if (sendMsgBuf(0x108180FE, 1, 8, data) == CAN_OK) {
        _hasNewTxValue[i] = false;
      } else {
        _errorCode |= HUAWEI_ERROR_CODE_TX;
      }
    }
  }

  if (_nextRequestMillis < millis()) {
    sendRequest();
    _nextRequestMillis = millis() + HUAWEI_DATA_REQUEST_INTERVAL_MS;
  }

}

byte HuaweiCanCommClass::sendMsgBuf(uint32_t identifier, uint8_t extd, uint8_t len, uint8_t *data) {
    auto _provider = PinMapping.get().charger.provider;
    switch (_provider) {
#ifdef USE_CHARGER_CAN0
        case Charger_Provider_t::CAN0:
            {
            twai_message_t tx_message;
            memcpy(tx_message.data, data, len);
            tx_message.extd = extd;
            tx_message.data_length_code = len;
            tx_message.identifier = identifier;

            if (twai_transmit(&tx_message, pdMS_TO_TICKS(1000)) == ESP_OK) {
                yield();
                return CAN_OK;
            }
            yield();
            MessageOutput.printf("%s Failed to queue message for transmission\r\n", TAG);
            }
            break;
#endif
#ifdef USE_CHARGER_MCP2515
        case Charger_Provider_t::MCP2515:
            return _CAN->sendMsgBuf(identifier, extd, len, data);
#endif
#ifdef USE_CHARGER_I2C
        case Charger_Provider_t::I2C0:
        case Charger_Provider_t::I2C1:
            i2c_can->sendMsgBuf(identifier, extd, len, data);
            return CAN_OK;
#endif
        default:;
    }

    return CAN_FAIL;
}

uint32_t HuaweiCanCommClass::getParameterValue(uint8_t parameter)
{
  std::lock_guard<std::mutex> lock(_mutex);
  uint32_t v = 0;
  if (parameter < HUAWEI_OUTPUT_CURRENT1_IDX) {
    v =  _recValues[parameter];
  }
  return v;
}

bool HuaweiCanCommClass::gotNewRxDataFrame(bool clear)
{
  std::lock_guard<std::mutex> lock(_mutex);
  bool b = false;
  b = _completeUpdateReceived;
  if (clear) {
    _completeUpdateReceived = false;
  }
  return b;
}

uint8_t HuaweiCanCommClass::getErrorCode(bool clear)
{
  std::lock_guard<std::mutex> lock(_mutex);
  uint8_t e = 0;
  e = _errorCode;
  if (clear) {
    _errorCode = 0;
  }
  return e;
}

void HuaweiCanCommClass::setParameterValue(uint16_t in, uint8_t parameterType)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (parameterType < HUAWEI_OFFLINE_CURRENT) {
    _txValues[parameterType] = in;
    _hasNewTxValue[parameterType] = true;
  }
}

// Private methods
// Requests current values from Huawei unit. Response is handled in onReceive
void HuaweiCanCommClass::sendRequest()
{
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //Send extended message
    if(sendMsgBuf(0x108040FE, 1, 8, data) != CAN_OK) {
        _errorCode |= HUAWEI_ERROR_CODE_RX;
    }
}

// *******************************************************
// Huawei CAN Controller
// *******************************************************

void HuaweiCanClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&HuaweiCanClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();
}

void HuaweiCanClass::updateSettings()
{
    const CONFIG_T& config = Configuration.get();

    _verboseLogging = config.Huawei.VerboseLogging;

    if (_initialized) {
      return;
    }

    if (!config.Huawei.Enabled) {
        return;
    }

    if (!HuaweiCanComm.init()) {
      MessageOutput.printf("%s::%s Error Initializing Huawei CAN communication...\r\n", TAG, __FUNCTION__);
      return;
    };

    _huaweiPower = PinMapping.get().charger.power;
    if (_huaweiPower >= 0) {
        pinMode(_huaweiPower, OUTPUT);
        digitalWrite(_huaweiPower, HIGH);
    }

    if (config.Huawei.Auto_Power_Enabled) {
      _mode = HUAWEI_MODE_AUTO_INT;
    }

    xTaskCreate(HuaweiCanCommunicationTask, "HUAWEI_CAN_0", 2048/*stack size*/,
        NULL/*params*/, 0/*prio*/, &_HuaweiCanCommunicationTaskHdl);

    MessageOutput.printf("%s::%s CAN Bus Controller initialized Successfully!\r\n", TAG, __FUNCTION__);
    _initialized = true;
}

RectifierParameters_t * HuaweiCanClass::get()
{
    return &_rp;
}

uint32_t HuaweiCanClass::getLastUpdate()
{
    return _lastUpdateReceivedMillis;
}

void HuaweiCanClass::processReceivedParameters()
{
    _rp.input_power = HuaweiCanComm.getParameterValue(HUAWEI_INPUT_POWER_IDX) / 1024.0;
    _rp.input_frequency = HuaweiCanComm.getParameterValue(HUAWEI_INPUT_FREQ_IDX) / 1024.0;
    _rp.input_current = HuaweiCanComm.getParameterValue(HUAWEI_INPUT_CURRENT_IDX) / 1024.0;
    _rp.output_power = HuaweiCanComm.getParameterValue(HUAWEI_OUTPUT_POWER_IDX) / 1024.0;
    _rp.efficiency = HuaweiCanComm.getParameterValue(HUAWEI_EFFICIENCY_IDX) / 1024.0;
    _rp.output_voltage = HuaweiCanComm.getParameterValue(HUAWEI_OUTPUT_VOLTAGE_IDX) / 1024.0;
    _rp.max_output_current = static_cast<float>(HuaweiCanComm.getParameterValue(HUAWEI_OUTPUT_CURRENT_MAX_IDX)) / MAX_CURRENT_MULTIPLIER;
    _rp.input_voltage = HuaweiCanComm.getParameterValue(HUAWEI_INPUT_VOLTAGE_IDX) / 1024.0;
    _rp.output_temp = HuaweiCanComm.getParameterValue(HUAWEI_OUTPUT_TEMPERATURE_IDX) / 1024.0;
    _rp.input_temp = HuaweiCanComm.getParameterValue(HUAWEI_INPUT_TEMPERATURE_IDX) / 1024.0;
    _rp.output_current = HuaweiCanComm.getParameterValue(HUAWEI_OUTPUT_CURRENT_IDX) / 1024.0;

    if (HuaweiCanComm.gotNewRxDataFrame(true)) {
      _lastUpdateReceivedMillis = millis();
    }
}


void HuaweiCanClass::loop()
{
  const CONFIG_T& config = Configuration.get();

  if (!config.Huawei.Enabled || !_initialized) {
      return;
  }

  processReceivedParameters();

  uint8_t com_error = HuaweiCanComm.getErrorCode(true);
  if (com_error & HUAWEI_ERROR_CODE_RX) {
    MessageOutput.printf("%s::%s Data request error\r\n", TAG, __FUNCTION__);
  }
  if (com_error & HUAWEI_ERROR_CODE_TX) {
    MessageOutput.printf("%s::%s Data set error\r\n", TAG, __FUNCTION__);
  }

  // Print updated data
  if (HuaweiCanComm.gotNewRxDataFrame(false) && _verboseLogging) {
    MessageOutput.printf("%s::%s In:  %.02fV, %.02fA, %.02fW\r\n", TAG, __FUNCTION__, _rp.input_voltage, _rp.input_current, _rp.input_power);
    MessageOutput.printf("%s::%s Out: %.02fV, %.02fA of %.02fA, %.02fW\r\n", TAG, __FUNCTION__, _rp.output_voltage, _rp.output_current, _rp.max_output_current, _rp.output_power);
    MessageOutput.printf("%s::%s Eff : %.01f%%, Temp in: %.01fC, Temp out: %.01fC\r\n", TAG, __FUNCTION__, _rp.efficiency * 100, _rp.input_temp, _rp.output_temp);
  }

  // Internal PSU power pin (slot detect) control
  if (_rp.output_current > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT) {
    _outputCurrentOnSinceMillis = millis();
  }
  if (_outputCurrentOnSinceMillis + HUAWEI_AUTO_MODE_SHUTDOWN_DELAY < millis() &&
      (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
    if (_huaweiPower >= 0) digitalWrite(_huaweiPower, 1);
  }

  // ***********************
  // Automatic power control
  // ***********************

  if (_mode == HUAWEI_MODE_AUTO_INT || _batteryEmergencyCharging) {

    // Set voltage limit in periodic intervals
    if ( _nextAutoModePeriodicIntMillis < millis()) {
      MessageOutput.printf("%s::%s Periodically setting voltage limit: %f\r\n",
        TAG, __FUNCTION__, config.Huawei.Auto_Power_Voltage_Limit);
      _setValue(config.Huawei.Auto_Power_Voltage_Limit, HUAWEI_ONLINE_VOLTAGE);
      _nextAutoModePeriodicIntMillis = millis() + 60000;
    }
  }
  // ***********************
  // Emergency charge
  // ***********************
  auto stats = Battery.getStats();
  if (config.Huawei.Emergency_Charge_Enabled && stats->getImmediateChargingRequest()) {
    _batteryEmergencyCharging = true;

    // Set output current
    float efficiency =  (_rp.efficiency > 0.5 ? _rp.efficiency : 1.0);
    float outputCurrent = efficiency * (config.Huawei.Auto_Power_Upper_Power_Limit / _rp.output_voltage);
    MessageOutput.printf("%s::%s Emergency Charge Output current %f\r\n", TAG, __FUNCTION__, outputCurrent);
    _setValue(outputCurrent, HUAWEI_ONLINE_CURRENT);
    return;
  }

  if (_batteryEmergencyCharging && !stats->getImmediateChargingRequest()) {
    // Battery request has changed. Set current to 0, wait for PSU to respond and then clear state
    _setValue(0, HUAWEI_ONLINE_CURRENT);
    if (_rp.output_current < 1) {
      _batteryEmergencyCharging = false;
    }
    return;
  }

  // ***********************
  // Automatic power control
  // ***********************

  if (_mode == HUAWEI_MODE_AUTO_INT ) {

    // Check if we should run automatic power calculation at all.
    // We may have set a value recently and still wait for output stabilization
    if (_autoModeBlockedTillMillis > millis()) {
      return;
    }

    // Re-enable automatic power control if the output voltage has dropped below threshold
    if(_rp.output_voltage < config.Huawei.Auto_Power_Enable_Voltage_Limit ) {
      _autoPowerEnabledCounter = 10;
    }


    // Check if inverter used by the power limiter is active
    std::shared_ptr<InverterAbstract> inverter =
        Hoymiles.getInverterBySerial(config.PowerLimiter.InverterId);

    if (inverter == nullptr && config.PowerLimiter.InverterId < INV_MAX_COUNT) {
        // we previously had an index saved as InverterId. fall back to the
        // respective positional lookup if InverterId is not a known serial.
        inverter = Hoymiles.getInverterByPos(config.PowerLimiter.InverterId);
    }

    if (inverter != nullptr) {
        if(inverter->isProducing()) {
          _setValue(0.0, HUAWEI_ONLINE_CURRENT);
          // Don't run auto mode for a second now. Otherwise we may send too much over the CAN bus
          _autoModeBlockedTillMillis = millis() + 1000;
          MessageOutput.printf("%s::%s Inverter is active, disable\r\n", TAG, __FUNCTION__);
          return;
        }
    }

    if (PowerMeter.getLastUpdate() > _lastPowerMeterUpdateReceivedMillis &&
        _autoPowerEnabledCounter > 0) {
        // We have received a new PowerMeter value. Also we're _autoPowerEnabled
        // So we're good to calculate a new limit

      _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();

      // Calculate new power limit
      float newPowerLimit = -1 * round(PowerMeter.getPowerTotal());
      float efficiency =  (_rp.efficiency > 0.5 ? _rp.efficiency : 1.0);

      // Powerlimit is the requested output power + permissable Grid consumption factoring in the efficiency factor
      newPowerLimit += _rp.output_power + config.Huawei.Auto_Power_Target_Power_Consumption / efficiency;

      if (_verboseLogging){
        MessageOutput.printf("%s::%s newPowerLimit: %f, output_power: %f\r\n",
            TAG, __FUNCTION__, newPowerLimit, _rp.output_power);
      }

      // Check whether the battery SoC limit setting is enabled
      if (config.Battery.Enabled && config.Huawei.Auto_Power_BatterySoC_Limits_Enabled) {
        uint8_t _batterySoC = Battery.getStats()->getSoC();
        // Sets power limit to 0 if the BMS reported SoC reaches or exceeds the user configured value
        if (_batterySoC >= config.Huawei.Auto_Power_Stop_BatterySoC_Threshold) {
          newPowerLimit = 0;
          if (_verboseLogging) {
            MessageOutput.printf("%s::%s Current battery SoC %i reached "
                    "stop threshold %i, set newPowerLimit to %f\r\n", TAG, __FUNCTION__, _batterySoC,
                    config.Huawei.Auto_Power_Stop_BatterySoC_Threshold, newPowerLimit);
          }
        }
      }

      if (newPowerLimit > config.Huawei.Auto_Power_Lower_Power_Limit) {

        // Check if the output power has dropped below the lower limit (i.e. the battery is full)
        // and if the PSU should be turned off. Also we use a simple counter mechanism here to be able
        // to ramp up from zero output power when starting up
        if (_rp.output_power < config.Huawei.Auto_Power_Lower_Power_Limit) {
          MessageOutput.printf("%s::%s Power and voltage limit reached. Disabling automatic power control ....\r\n", TAG, __FUNCTION__);
          _autoPowerEnabledCounter--;
          if (_autoPowerEnabledCounter == 0) {
            _autoPowerEnabled = false;
            _setValue(0, HUAWEI_ONLINE_CURRENT);
            return;
          }
        } else {
          _autoPowerEnabledCounter = 10;
        }

        // Limit power to maximum
        if (newPowerLimit > config.Huawei.Auto_Power_Upper_Power_Limit) {
          newPowerLimit = config.Huawei.Auto_Power_Upper_Power_Limit;
        }

        // Calculate output current
        float calculatedCurrent = efficiency * (newPowerLimit / _rp.output_voltage);

        // Limit output current to value requested by BMS
        float permissableCurrent = stats->getChargeCurrentLimitation() - (stats->getChargeCurrent() - _rp.output_current); // BMS current limit - current from other sources, e.g. Victron MPPT charger
        float outputCurrent = std::min(calculatedCurrent, permissableCurrent);
        outputCurrent= outputCurrent > 0 ? outputCurrent : 0;

        if (_verboseLogging) {
            MessageOutput.printf("%s::%s Setting output current to %.2fA. This is the lower value of calculated %.2fA and BMS permissable %.2fA currents\r\n",
                TAG, __FUNCTION__, outputCurrent, calculatedCurrent, permissableCurrent);
        }
        _autoPowerEnabled = true;
        _setValue(outputCurrent, HUAWEI_ONLINE_CURRENT);

        // Don't run auto mode some time to allow for output stabilization after issuing a new value
        _autoModeBlockedTillMillis = millis() + 2 * HUAWEI_DATA_REQUEST_INTERVAL_MS;
      } else {
        // requested PL is below minium. Set current to 0
        _autoPowerEnabled = false;
        _setValue(0.0, HUAWEI_ONLINE_CURRENT);
      }
    }
  }
}

void HuaweiCanClass::setValue(float in, uint8_t parameterType)
{
  if (_mode != HUAWEI_MODE_AUTO_INT) {
    _setValue(in, parameterType);
  }
}

void HuaweiCanClass::_setValue(float in, uint8_t parameterType)
{

    const CONFIG_T& config = Configuration.get();

    if (!config.Huawei.Enabled) {
        return;
    }

    uint16_t value;

    if (in < 0) {
      MessageOutput.printf("%s::%s Error: Tried to set voltage/current to negative value %f\r\n", TAG, __FUNCTION__, in);
    }

    // Start PSU if needed
    if (in > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT && parameterType == HUAWEI_ONLINE_CURRENT &&
        (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
      if (_huaweiPower >= 0) digitalWrite(_huaweiPower, 0);
      _outputCurrentOnSinceMillis = millis();
    }

    if (parameterType == HUAWEI_OFFLINE_VOLTAGE || parameterType == HUAWEI_ONLINE_VOLTAGE) {
        value = in * 1024;
    } else if (parameterType == HUAWEI_OFFLINE_CURRENT || parameterType == HUAWEI_ONLINE_CURRENT) {
        value = in * MAX_CURRENT_MULTIPLIER;
    } else {
        return;
    }

    HuaweiCanComm.setParameterValue(value, parameterType);
}

void HuaweiCanClass::setMode(uint8_t mode) {
  const CONFIG_T& config = Configuration.get();

  if (!config.Huawei.Enabled) {
      return;
  }

  if(mode == HUAWEI_MODE_OFF) {
    if (_huaweiPower >= 0) digitalWrite(_huaweiPower, 1);
    _mode = HUAWEI_MODE_OFF;
  }
  if(mode == HUAWEI_MODE_ON) {
    if (_huaweiPower >= 0) digitalWrite(_huaweiPower, 0);
    _mode = HUAWEI_MODE_ON;
  }

  if (mode == HUAWEI_MODE_AUTO_INT && !config.Huawei.Auto_Power_Enabled ) {
    MessageOutput.printf("%s::%s WARNING: Trying to setmode to internal automatic power control without being enabled in the UI. Ignoring command\r\n",
        TAG, __FUNCTION__);
    return;
  }

  if (_mode == HUAWEI_MODE_AUTO_INT && mode != HUAWEI_MODE_AUTO_INT) {
    _autoPowerEnabled = false;
    _setValue(0, HUAWEI_ONLINE_CURRENT);
  }

  if(mode == HUAWEI_MODE_AUTO_EXT || mode == HUAWEI_MODE_AUTO_INT) {
    _mode = mode;
  }
}

#endif
