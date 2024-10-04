// SPDX-License-Identifier: GPL-2.0-or-later

#include "mcp2515_spi.h"

extern SemaphoreHandle_t paramLock;
#define SPI_PARAM_LOCK() \
    do {                 \
    } while (xSemaphoreTake(paramLock, portMAX_DELAY) != pdPASS)
#define SPI_PARAM_UNLOCK() xSemaphoreGive(paramLock)

static void IRAM_ATTR pre_cb(spi_transaction_t *trans) {
    gpio_set_level(*reinterpret_cast<gpio_num_t*>(trans->user), 0);
}

static void IRAM_ATTR post_cb(spi_transaction_t *trans) {
    gpio_set_level(*reinterpret_cast<gpio_num_t*>(trans->user), 1);
}

void MCP2515SPIClass::spi_init(const int8_t pin_miso, const int8_t pin_mosi, const int8_t pin_clk, const int8_t pin_cs, const int32_t spi_speed)
{
    if (paramLock == NULL) paramLock = xSemaphoreCreateMutex();
    if (paramLock == NULL) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    auto bus_config = std::make_shared<SpiBusConfig>(
        static_cast<gpio_num_t>(pin_mosi),
        static_cast<gpio_num_t>(pin_miso),
        static_cast<gpio_num_t>(pin_clk)
    );

    spi_device_interface_config_t device_config {
        .command_bits = 0, // set by transactions individually
        .address_bits = 0, // set by transactions individually
        .dummy_bits = 0,
        .mode = 0, // SPI mode 0
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 2, // only 1 pre and post cycle would be required for register access
        .cs_ena_posttrans = static_cast<uint8_t>(2 * spi_speed / 1000000), // >2 us
        .clock_speed_hz = spi_speed, // 10000000, // 10mhz
        .input_delay_ns = 0,
        .spics_io_num = -1, // CS handled by callbacks
        .flags = 0,
        .queue_size = 1,
        .pre_cb = pre_cb,
        .post_cb = post_cb,
    };

    spi = SpiManagerInst.alloc_device("SPI", bus_config, device_config);
    if (!spi)
        ESP_ERROR_CHECK(ESP_FAIL);

    cs_reg = static_cast<gpio_num_t>(pin_cs);
    ESP_ERROR_CHECK(gpio_reset_pin(cs_reg));
    ESP_ERROR_CHECK(gpio_set_level(cs_reg, 1));
    ESP_ERROR_CHECK(gpio_set_direction(cs_reg, GPIO_MODE_OUTPUT));
}

void MCP2515SPIClass::spi_deinit(void)
{
    spi_bus_remove_device(spi);
}

void MCP2515SPIClass::spi_reset(void)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .cmd = 0,
        .addr = 0,
        .length = 8, // 1 bits = 1 bytes
        .rxlength = 0,
        .user = &cs_reg,
        .tx_data = {INSTRUCTION_RESET,0,0,0},
        .rx_data = {0,0,0,0}
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();
}

uint8_t MCP2515SPIClass::spi_readRegister(const REGISTER_t reg)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .cmd = 0,
        .addr = 0,
        .length = (3 * 8), // 24 bits = 3 bytes
        .rxlength = 0, // default to length 24 bits = 3 bytes
        .user = &cs_reg,
        .tx_data = {INSTRUCTION_READ, reg, 0x00},
        .rx_data = {0,0,0,0}
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();

    return trans.rx_data[2];
}

void MCP2515SPIClass::spi_readRegisters(const REGISTER_t reg, uint8_t values[], const uint8_t n)
{
    uint8_t tx_data[n + 2] = { INSTRUCTION_READ, reg };
    uint8_t rx_data[n + 2];

    spi_transaction_t trans = {
        .flags = 0,  // use tx_buffer and rx_buffer
        .cmd = 0,
        .addr = 0,
        .length = ((2 + ((size_t)n)) * 8),
        .rxlength = 0,
        .user = &cs_reg,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();
    for (uint8_t i = 0; i < n; i++) {
        values[i] = rx_data[i+2];
    }
}

void MCP2515SPIClass::spi_setRegister(const REGISTER_t reg, const uint8_t value)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .cmd = 0,
        .addr = 0,
        .length = (3 * 8),
        .rxlength = 0,
        .user = &cs_reg,
        .tx_data = { INSTRUCTION_WRITE, reg, value },
        .rx_data = {0,0,0,0}
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();
}

void MCP2515SPIClass::spi_setRegisters(const REGISTER_t reg, const uint8_t values[], const uint8_t n)
{
    uint8_t data[n + 2];

    data[0] = INSTRUCTION_WRITE;
    data[1] = reg;

    for (uint8_t i=0; i<n; i++) {
        data[i+2] = values[i];
    }

    spi_transaction_t trans = {
        .flags = 0, // use tx_buffer and rx_buffer (point to nullptr)
        .cmd = 0,
        .addr = 0,
        .length = ((2 + ((size_t)n)) * 8),
        .rxlength = 0,
        .user = &cs_reg,
        .tx_buffer = data,
        .rx_buffer = nullptr
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();
}

void MCP2515SPIClass::spi_modifyRegister(const REGISTER_t reg, const uint8_t mask, const uint8_t data)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .cmd = 0,
        .addr = 0,
        .length = (4 * 8),
        .rxlength = 0,
        .user = &cs_reg,
        .tx_data = { INSTRUCTION_BITMOD, reg, mask, data },
        .rx_data = {0,0,0,0}
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();
}

uint8_t MCP2515SPIClass::spi_getStatus(void)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .cmd = 0,
        .addr = 0,
        .length = (2 * 8),
        .rxlength = 0,
        .user = &cs_reg,
        .tx_data = { INSTRUCTION_READ_STATUS, 0x00 },
        .rx_data = {0,0,0,0}
    };

    SPI_PARAM_LOCK();
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &trans));
    SPI_PARAM_UNLOCK();

    return trans.rx_data[1];
}
