#include "mcp2515_init_spi.h"

spi_device_handle_t mcp2515_init_spi(const int8_t pin_miso, const int8_t pin_mosi, const int8_t pin_clk, const int8_t pin_cs, const int32_t spi_speed)
{
    spi_device_handle_t spi;

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
        .spics_io_num = pin_cs,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 1,
        .pre_cb = nullptr,
        .post_cb = nullptr,
    };

    spi = SpiManagerInst.alloc_device("Shared SPI", bus_config, device_config);
    if (!spi)
        ESP_ERROR_CHECK(ESP_FAIL);

    return spi;
}
