// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)

#include "BatteryCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include "SpiManager.h"

bool BatteryCanReceiver::init(char const* providerName)
{
    snprintf(_providerName, sizeof(_providerName), "[%s %s]", providerName, PinMapping.get().battery.providerName);

    MessageOutput.printf("%s Initialize interface...", _providerName);

    if (!PinMapping.isValidBatteryConfig()) {
        MessageOutput.println(" Invalid pin config");
        return false;
    }

    const auto &pin = PinMapping.get().battery;

    switch (pin.provider) {
#ifdef USE_BATTERY_MCP2515
        case Battery_Provider_t::MCP2515:
            {
            int rc;

            MessageOutput.printf(" clk = %d, miso = %d, mosi = %d, cs = %d, irq = %d\r\n",
                pin.mcp2515.clk, pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.cs, pin.mcp2515.irq);

            _CAN = new MCP2515Class(pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.clk, pin.mcp2515.cs);

            auto frequency = Configuration.get().MCP2515.Controller_Frequency;
            auto mcp_frequency = MCP_8MHZ;
            if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
            else if (20000000UL == frequency) { mcp_frequency = MCP_20MHZ; }
            else if (8000000UL != frequency) {
                MessageOutput.printf("Unknown frequency %u Hz, using 8 MHz\r\n", frequency);
            }
            MessageOutput.printf("MCP2515 Quarz = %u Mhz\r\n", (unsigned int)(frequency/1000000UL));
            if ((rc = _CAN->initMCP2515(MCP_ANY, CAN_500KBPS, mcp_frequency)) != CAN_OK) {
                MessageOutput.printf("%s MCP2515 failed to initialize. Error code: %d\r\n", _providerName, rc);
                return false;
            }
/*
            auto spi_bus = SpiManagerInst.claim_bus_arduino();
            ESP_ERROR_CHECK(spi_bus ? ESP_OK : ESP_FAIL);

            MessageOutput.printf("Init SPI... ");
            SPI = new SPIClass(*spi_bus);
            SPI->begin(pin.mcp2515.clk, pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.cs);
            MessageOutput.println("done.");

            pinMode(pin.mcp2515.cs, OUTPUT);
            digitalWrite(pin.mcp2515.cs, HIGH);
*/
            _mcp2515_irq = pin.mcp2515.irq;
            pinMode(_mcp2515_irq, INPUT_PULLUP);

//            _CAN->setBitrate(CAN_500KBPS, mcp_frequency);

/*
            if (!_CAN) {
                MessageOutput.printf(", init MCP_CAN... ");
                _CAN = new MCP_CAN(SPI, pin.mcp2515.cs);
                MessageOutput.println("done.");
            }
            if (!_CAN->begin(MCP_STDEXT, CAN_500KBPS, mcp_frequency) == CAN_OK) {
                MessageOutput.println("MCP2515 chip not initialized");
                //SPI->end();
                return false;
            }
            const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
            const uint32_t myFilter = 0x1081407F;       // filter for this message only
            _CAN->init_Mask(0, 1, myMask);
            _CAN->init_Filt(0, 1, myFilter);
            _CAN->init_Mask(1, 1, myMask);
*/
            // Change to normal mode to allow messages to be transmitted
            _CAN->setMode(MCP_NORMAL);
            }
            break;
#endif
#ifdef USE_BATTERY_CAN0
        case Battery_Provider_t::CAN0:
            {
            MessageOutput.printf(" rx = %d, tx = %d\r\n", pin.can0.rx, pin.can0.tx);

            auto tx = static_cast<gpio_num_t>(pin.can0.tx);
            auto rx = static_cast<gpio_num_t>(pin.can0.rx);
            twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);
            // g_config.bus_off_io = (gpio_num_t)can0_stb;
#if defined(BOARD_HAS_PSRAM)
            g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;
#endif
            // Initialize configuration structures using macro initializers
            twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
            twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

            // Install TWAI driver
            MessageOutput.print("Twai driver install");
            esp_err_t twaiLastResult = twai_driver_install(&g_config, &t_config, &f_config);
            switch (twaiLastResult) {
                case ESP_OK:
                    MessageOutput.print("ed");
                    break;
                case ESP_ERR_INVALID_ARG:
                    MessageOutput.println(" - invalid arg");
                    return false;
                case ESP_ERR_NO_MEM:
                    MessageOutput.println(" - no memory");
                    return false;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.println(" - invalid state");
                    return false;
                default:;
                    MessageOutput.println(" failed.");
                    return false;
            }

            // Start TWAI driver
            MessageOutput.print(", start");
            twaiLastResult = twai_start();
            switch (twaiLastResult) {
                case ESP_OK:
                    MessageOutput.print("ed");
                    break;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.println(" - invalid state.");
                    return false;
                default:;
                    MessageOutput.println(" - failed.");
                    return false;
            }
            }
            break;
#endif
#ifdef USE_BATTERY_I2C
        case Battery_Provider_t::I2C0:
        case Battery_Provider_t::I2C1:
            {
            auto scl = pin.i2c.scl;
            auto sda = pin.i2c.sda;

            MessageOutput.printf("I2C CAN Bus @ I2C%d scl = %d, sda = %d\r\n", pin.provider == Battery_Provider_t::I2C0?0:1, scl, sda);

            i2c_can = new I2C_CAN(pin.provider == Battery_Provider_t::I2C0?&Wire:&Wire1, 0x25, scl, sda, 400000UL);     // Set I2C Address

            int i = 10;
            while (CAN_OK != i2c_can->begin(I2C_CAN_500KBPS))    // init can bus : baudrate = 500k
            {
                delay(200);
                if (--i) continue;

                MessageOutput.println("CAN Bus FAIL!");
                return false;
            }
/*
            const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
            const uint32_t myFilter = 0x1081407F;       // filter for this message only
            i2c_can->init_Mask(0, 1, myMask);
            i2c_can->init_Filt(0, 1, myFilter);
            i2c_can->init_Mask(1, 1, myMask);
*/
            MessageOutput.println("I2C CAN Bus OK!");
            }
            break;
#endif
        default:;
            MessageOutput.println(" Error: no IO provider configured");
            return false;
    }

    MessageOutput.println(" Done");

    _initialized = true;

    return true;
}

void BatteryCanReceiver::deinit()
{
    const auto &pin = PinMapping.get().battery;

    switch(pin.provider) {
#ifdef USE_BATTERY_MCP2515
        case Battery_Provider_t::MCP2515:
            MessageOutput.printf("%s driver stopped", _providerName);

//            if (SPI) SPI->end();
            if (_CAN) delete _CAN;
/*
            pinMode(pin.mcp2515_cs, INPUT);
            pinMode(pin.mcp2515_mosi, INPUT);
            pinMode(pin.mcp2515_miso, INPUT);
            pinMode(pin.mcp2515_clk, INPUT);
            pinMode(pin.mcp2515_irq, INPUT);
*/
            break;
#endif
#ifdef USE_BATTERY_CAN0
        case Battery_Provider_t::CAN0:
            // Stop TWAI driver
            MessageOutput.printf("%s Twai driver ", _providerName);
            switch (twai_stop()) {
                case ESP_OK:
                    MessageOutput.printf("stopped");
                    break;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.print(" - invalid state");
                    break;
            }

            // Uninstall TWAI driver
            switch (twai_driver_uninstall()) {
                case ESP_OK:
                    MessageOutput.print(" and uninstalled");
                    break;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.print(", uninstall - invalid state");
            }
            break;
#endif
#ifdef USE_BATTERY_I2C
        case Battery_Provider_t::I2C0:
        case Battery_Provider_t::I2C1:
            {

            }
            break;
#endif
        default:;
    }
    MessageOutput.println();

    _initialized = false;
}

void BatteryCanReceiver::loop()
{
    const auto _provider = PinMapping.get().battery.provider;

    twai_message_t rx_message;

    switch (_provider) {
#ifdef USE_BATTERY_MCP2515
        case Battery_Provider_t::MCP2515:
            if(digitalRead(_mcp2515_irq)) return;  // If CAN0_INT pin is low, read receive buffer

//            _CAN->readMsgBuf(&rx_message.identifier, &rx_message.data_length_code, rx_message.data);      // Read data: len = data length, buf = data byte(s)
            _CAN->readMsgBuf((can_message_t*)&rx_message);      // Read data: len = data length, buf = data byte(s)

            if (_verboseLogging) {
                if((rx_message.identifier & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
                    MessageOutput.printf("Extended ID: 0x%.8" PRIx32 " DLC: %1d  Data:", rx_message.identifier & 0x1FFFFFFF, rx_message.data_length_code);
                else
                    MessageOutput.printf("Standard ID: 0x%.3" PRIx32 " DLC: %1d  Data:", rx_message.identifier, rx_message.data_length_code);
            }

            if ((rx_message.identifier & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
                MessageOutput.printf(" REMOTE REQUEST FRAME %s\r\n", rx_message.data);
                return;
            }
            break;
#endif
#ifdef USE_BATTERY_CAN0
        case Battery_Provider_t::CAN0:
            {

            // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
            twai_status_info_t status_info;
            esp_err_t twaiLastResult = twai_get_status_info(&status_info);
            if (twaiLastResult != ESP_OK) {
                MessageOutput.printf("%s Twai driver get status ", _providerName);
                switch (twaiLastResult) {
                case ESP_ERR_INVALID_ARG:
                    MessageOutput.println("- invalid arg");
                    break;
                case ESP_ERR_INVALID_STATE:
                    MessageOutput.println("- invalid state");
                    break;
                default:;
                    MessageOutput.println("- failure");
                }
                return;
            }

            if (status_info.msgs_to_rx == 0) {
                return;
            }

            // Wait for message to be received, function is blocking
            if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
                MessageOutput.printf("%s Failed to receive message\r\n", _providerName);
                return;
            }
            }
            break;
#endif
#ifdef USE_BATTERY_I2C
        case Battery_Provider_t::I2C0:
        case Battery_Provider_t::I2C1:
            if (CAN_MSGAVAIL != i2c_can->checkReceive()) return;

            if (CAN_OK != i2c_can->readMsgBuf(&rx_message.data_length_code, rx_message.data)) {    // read data,  len: data length, buf: data buf
                MessageOutput.println("I2C CAN nothing received");
                return;
            }

            if (rx_message.data_length_code > 8) {
                MessageOutput.printf("I2C CAN received %d bytes\r\n", rx_message.data_length_code);
                return;
            }

            if (rx_message.data_length_code == 0) return;

            rx_message.identifier = i2c_can->getCanId();
            rx_message.extd = i2c_can->isExtendedFrame();
            rx_message.rtr = i2c_can->isRemoteRequest();
            break;
#endif
        default:;
    }

    if (_verboseLogging) {
        MessageOutput.printf("%s Received CAN message: 0x%04X -", _providerName, rx_message.identifier);

        for (int i = 0; i < rx_message.data_length_code; i++) {
            MessageOutput.printf(" %02X", rx_message.data[i]);
        }

        MessageOutput.printf("\r\n");
    }

    onMessage(rx_message);
}

uint8_t BatteryCanReceiver::readUnsignedInt8(uint8_t *data)
{
    return data[0];
}

uint16_t BatteryCanReceiver::readUnsignedInt16(uint8_t *data)
{
    return (data[1] << 8) + data[0];
}

int16_t BatteryCanReceiver::readSignedInt16(uint8_t *data)
{
    return this->readUnsignedInt16(data);
}

int32_t BatteryCanReceiver::readSignedInt24(uint8_t *data)
{
    return (data[2] << 16) | (data[1] << 8) | data[0];
}

uint32_t BatteryCanReceiver::readUnsignedInt32(uint8_t *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

float BatteryCanReceiver::scaleValue(int32_t value, float factor)
{
    return value * factor;
}

bool BatteryCanReceiver::getBit(uint8_t value, uint8_t bit)
{
    return (value & (1 << bit)) >> bit;
}

#endif
