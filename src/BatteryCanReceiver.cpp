// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)

#include "BatteryCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include "SPIPortManager.h"

bool BatteryCanReceiver::init(char const* providerName)
{
    const char* help[7] = {"unknown", "CAN0", "MCP2515", "I2C0", "I2C1", "RS232", "RS485"};
    static char helperString[32];
    snprintf(helperString, 31, "[%s %s]", providerName, help[(int)PinMapping.get().battery.provider]);
    _providerName = helperString;

    MessageOutput.printf("%s Initialize interface...", _providerName);

    const auto &pin = PinMapping.get().battery;

    if (pin.provider == Battery_Provider_t::MCP2515)
    {
        MessageOutput.printf(" clk = %d, miso = %d, mosi = %d, cs = %d, irq = %d",
            pin.mcp2515.clk, pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.cs, pin.mcp2515.irq);

        if (pin.mcp2515.clk < 0 || pin.mcp2515.miso < 0 || pin.mcp2515.mosi < 0 || pin.mcp2515.cs < 0 || pin.mcp2515.irq < 0) {
            MessageOutput.println(", Invalid pin config");
            return false;
        }

        _mcp2515_irq = pin.mcp2515.irq;

        auto oSPInum = SPIPortManager.allocatePort("MCP2515");
        if (!oSPInum) { return false; }
        MessageOutput.printf(", init SPI... ");
        SPI = new SPIClass(*oSPInum);
        SPI->begin(pin.mcp2515.clk, pin.mcp2515.miso, pin.mcp2515.mosi, pin.mcp2515.cs);
        MessageOutput.printf("done");

        pinMode(pin.mcp2515.cs, OUTPUT);
        digitalWrite(pin.mcp2515.cs, HIGH);

        pinMode(_mcp2515_irq, INPUT_PULLUP);

        auto frequency = Configuration.get().MCP2515.Controller_Frequency;
        auto mcp_frequency = MCP_8MHZ;
        if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
        else if (20000000UL == frequency) { mcp_frequency = MCP_20MHZ; }
        else if (8000000UL != frequency) {
            MessageOutput.printf("%s unknown frequency %u Hz, using 8 MHz\r\n", _providerName, frequency);
            //SPI->end();
            return false;
        }
        MessageOutput.printf(", MCP2515 Quarz = %u Mhz", (unsigned int)(frequency/1000000UL));

        if (!_CAN) {
            MessageOutput.printf(", init MCP_CAN... ");
            _CAN = new MCP_CAN(SPI, pin.mcp2515.cs);
            MessageOutput.printf("done");
        }
        if (!_CAN->begin(MCP_STDEXT, CAN_500KBPS, mcp_frequency) == CAN_OK) {
            MessageOutput.println(", MCP2515 chip not initialized");
            //SPI->end();
            return false;
        }

/*        const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
        const uint32_t myFilter = 0x1081407F;       // filter for this message only
        _CAN->init_Mask(0, 1, myMask);
        _CAN->init_Filt(0, 1, myFilter);
        _CAN->init_Mask(1, 1, myMask);
*/

        // Change to normal mode to allow messages to be transmitted
        _CAN->setMode(MCP_NORMAL);

    } else if (pin.provider == Battery_Provider_t::CAN0) {

        MessageOutput.printf(" rx = %d, tx = %d", pin.can0.rx, pin.can0.tx);

        if (PinMapping.isValidBatteryConfig()) {
            MessageOutput.println(" Invalid pin config");
            return false;
        }

        auto tx = static_cast<gpio_num_t>(pin.can0.tx);
        auto rx = static_cast<gpio_num_t>(pin.can0.rx);
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);

        // Initialize configuration structures using macro initializers
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        // Install TWAI driver
        esp_err_t twaiLastResult = twai_driver_install(&g_config, &t_config, &f_config);
        switch (twaiLastResult) {
            case ESP_OK:
                MessageOutput.print(" Twai driver installed");
                break;
            case ESP_ERR_INVALID_ARG:
                MessageOutput.println(" Twai driver install - invalid arg");
                return false;
            case ESP_ERR_NO_MEM:
                MessageOutput.println(" Twai driver install - no memory");
                return false;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.println(" Twai driver install - invalid state");
                return false;
        }

        // Start TWAI driver
        twaiLastResult = twai_start();
        switch (twaiLastResult) {
            case ESP_OK:
                MessageOutput.print(" and started");
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.println(" Twai driver start - invalid state");
                return false;
        }
    } else if (pin.provider == Battery_Provider_t::I2C0 || pin.provider == Battery_Provider_t::I2C1) {
        MessageOutput.println(" I2C CAN Bus Provider not yet implemented");
        return false;
    }

    MessageOutput.println(" Done");

    _initialized = true;

    return true;
}

void BatteryCanReceiver::deinit()
{
    const auto &pin = PinMapping.get().battery;

    if (pin.provider == Battery_Provider_t::MCP2515){
        MessageOutput.printf("%s driver stopped", _providerName);

        if (SPI) SPI->end();
/*
        pinMode(pin.mcp2515_cs, INPUT);
        pinMode(pin.mcp2515_mosi, INPUT);
        pinMode(pin.mcp2515_miso, INPUT);
        pinMode(pin.mcp2515_clk, INPUT);
        pinMode(pin.mcp2515_irq, INPUT);
*/
    } else if (pin.provider == Battery_Provider_t::CAN0){
        // Stop TWAI driver
        esp_err_t twaiLastResult = twai_stop();
        MessageOutput.printf("%s Twai driver ", _providerName);
        switch (twaiLastResult) {
            case ESP_OK:
                MessageOutput.printf("stopped");
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.printf(" - invalid state");
                break;
        }

        // Uninstall TWAI driver
        twaiLastResult = twai_driver_uninstall();
        switch (twaiLastResult) {
            case ESP_OK:
                MessageOutput.print(" uninstalled");
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.print(" uninstall - invalid state");
                break;
        }
    }

    MessageOutput.println();

    _initialized = false;
}

void BatteryCanReceiver::loop()
{
    const auto pin = PinMapping.get().battery;

    twai_message_t rx_message;

    if (pin.provider == Battery_Provider_t::MCP2515)
    {
        if(!digitalRead(_mcp2515_irq))                         // If CAN0_INT pin is low, read receive buffer
        {
            _CAN->readMsgBuf(&rx_message.identifier, &rx_message.data_length_code, rx_message.data);      // Read data: len = data length, buf = data byte(s)

            if((rx_message.identifier & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
                MessageOutput.printf("Extended ID: 0x%.8" PRIx32 " DLC: %1d  Data:", rx_message.identifier & 0x1FFFFFFF, rx_message.data_length_code);
            else
                MessageOutput.printf("Standard ID: 0x%.3" PRIx32 " DLC: %1d  Data:", rx_message.identifier, rx_message.data_length_code);

            if((rx_message.identifier & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
                MessageOutput.printf(" REMOTE REQUEST FRAME %s\r\n", rx_message.data);
            } else {

                if (_verboseLogging) {
                    MessageOutput.printf("%s Received CAN message: 0x%04X -", _providerName, rx_message.identifier);

                    for (int i = 0; i < rx_message.data_length_code; i++) {
                        MessageOutput.printf(" %02X", rx_message.data[i]);
                    }

                    MessageOutput.printf("\r\n");
                }

                onMessage(rx_message);
            }
            return;
        }

    } else if (pin.provider == Battery_Provider_t::CAN0) {

        // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
        twai_status_info_t status_info;
        esp_err_t twaiLastResult = twai_get_status_info(&status_info);
        if (twaiLastResult != ESP_OK) {
            switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                MessageOutput.printf("%s Twai driver get status - invalid arg\r\n", _providerName);
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.printf("%s Twai driver get status - invalid state\r\n", _providerName);
                break;
            }
            return;
        }
        if (status_info.msgs_to_rx == 0) {
            return;
        }

        // Wait for message to be received, function is blocking
        twai_message_t rx_message;
        if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
            MessageOutput.printf("%s Failed to receive message\r\n", _providerName);
            return;
        }
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

uint32_t BatteryCanReceiver::readUnsignedInt32(uint8_t *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

float BatteryCanReceiver::scaleValue(int16_t value, float factor)
{
    return value * factor;
}

bool BatteryCanReceiver::getBit(uint8_t value, uint8_t bit)
{
    return (value & (1 << bit)) >> bit;
}

#endif
