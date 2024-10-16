#include <SpiManager.h>
#include <mcp2515_spi.h>

spi_device_handle_t mcp2515_init_spi(const int8_t pin_miso, const int8_t pin_mosi, const int8_t pin_clk, const int8_t pin_cs, const int32_t spi_speed = MCP2515_SPI_SPEED);
