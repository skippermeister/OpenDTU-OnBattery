// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_PYLONTECH_CAN_RECEIVER

#include "BatteryCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>

bool BatteryCanReceiver::init(char const* providerName)
{
    _verboseLogging = Battery._verboseLogging;
    _providerName = providerName;

    MessageOutput.printf("%s Initialize interface...", _providerName);

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf(" Interface rx = %d, tx = %d", pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.println(" Invalid pin config");
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);
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

    MessageOutput.println(" Done");

    return true;
}

void BatteryCanReceiver::deinit()
{
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
    MessageOutput.println();
}

void BatteryCanReceiver::loop()
{
    // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
    twai_status_info_t status_info;
    esp_err_t twaiLastResult = twai_get_status_info(&status_info);
    if (twaiLastResult != ESP_OK) {
        switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                MessageOutput.print("%s Twai driver get status - invalid arg\r\n", _providerName);
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.print("%s Twai driver get status - invalid state\r\n", _providerName);
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

    onMessage(rx_message);
}

uint16_t BatteryCanReceiver::readUnsignedInt16(uint8_t *data)
{
    uint8_t bytes[2];
    bytes[0] = *data;
    bytes[1] = *(data + 1);
    return (bytes[1] << 8) + bytes[0];
}

int16_t BatteryCanReceiver::readSignedInt16(uint8_t *data)
{
    return this->readUnsignedInt16(data);
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
