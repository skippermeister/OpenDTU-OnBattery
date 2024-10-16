// SPDX-License-Identifier: GPL-2.0-or-later

#include "mcp2515_spi.h"

#define SPI_PARAM_LOCK() \
    do {                 \
    } while (xSemaphoreTake(paramLock, portMAX_DELAY) != pdPASS)
#define SPI_PARAM_UNLOCK() xSemaphoreGive(paramLock)

void MCP2515SPIClass::spi_init(void)
{
    if (paramLock == NULL) paramLock = xSemaphoreCreateMutex();
    if (paramLock == NULL) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    if (!connection_check_interrupt(_pin_irq))
        ESP_ERROR_CHECK(ESP_FAIL);

    // Return to default state once again after connection check
    ESP_ERROR_CHECK(gpio_reset_pin(_pin_irq));
    ESP_ERROR_CHECK(gpio_set_direction(_pin_irq, GPIO_MODE_INPUT));
}

bool MCP2515SPIClass::connection_check_interrupt(gpio_num_t pin_irq)
{
    gpio_set_direction(pin_irq, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin_irq, GPIO_PULLDOWN_ONLY);
    int level = gpio_get_level(pin_irq);

    // Interrupt line must be high
    return level == 1;
}

void MCP2515SPIClass::spi_deinit(void)
{
    spi_bus_remove_device(_spi);
}

void MCP2515SPIClass::spi_reset(void)
{
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD, // SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
            .cmd = INSTRUCTION_RESET,
            .addr = 0,
            .length = 0,
            .rxlength = 0,
            .user = 0,
            .tx_buffer = nullptr,
            .rx_buffer = nullptr
        },
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();
}

uint8_t MCP2515SPIClass::spi_readRegister(const REGISTER_t reg)
{
    uint8_t data;
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR,
            .cmd = INSTRUCTION_READ,
            .addr = reg,
            .length = 0,
            .rxlength = 8,
            .user = 0,
            .tx_buffer = nullptr,
            .rx_buffer = &data
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();

    return data;
}

void MCP2515SPIClass::spi_readRegisters(const REGISTER_t reg, uint8_t values[], const uint8_t n)
{
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR,  // use tx_buffer and rx_buffer
            .cmd = INSTRUCTION_READ,
            .addr = reg,
            .length = 0,
            .rxlength = ((size_t)n) * 8,
            .user = 0,
            .tx_buffer = nullptr,
            .rx_buffer = values
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();
}

void MCP2515SPIClass::spi_setRegister(const REGISTER_t reg, const uint8_t value)
{
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR, // SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
            .cmd = INSTRUCTION_WRITE,
            .addr = reg,
            .length = 8,
            .rxlength = 0,
            .user = 0,
            .tx_buffer = &value,
            .rx_buffer = nullptr
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();
}

void MCP2515SPIClass::spi_setRegisters(const REGISTER_t reg, const uint8_t values[], const uint8_t n)
{
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR, // use tx_buffer and rx_buffer (point to nullptr)
            .cmd = INSTRUCTION_WRITE,
            .addr = reg,
            .length = ((size_t)n) * 8,
            .rxlength = 0,
            .user = 0,
            .tx_buffer = values,
            .rx_buffer = nullptr
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();
}

void MCP2515SPIClass::spi_modifyRegister(const REGISTER_t reg, const uint8_t mask, const uint8_t data)
{
    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_TXDATA,
            .cmd = INSTRUCTION_BITMOD,
            .addr = reg,
            .length = 2 * 8,
            .rxlength = 0,
            .user = 0,
            .tx_data = { mask, data },
            .rx_buffer = nullptr
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();
}

uint8_t MCP2515SPIClass::spi_getStatus(void)
{
    uint8_t data;

    spi_transaction_ext_t trans = {
        .base {
            .flags = SPI_TRANS_VARIABLE_CMD, // SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
            .cmd = INSTRUCTION_READ_STATUS,
            .addr = 0,
            .length = 0,
            .rxlength = 8,
            .user = 0,
            .tx_buffer = nullptr,
            .rx_buffer = &data
        },
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(_spi, reinterpret_cast<spi_transaction_t*>(&trans)));
    SPI_PARAM_UNLOCK();

    return data;
}
