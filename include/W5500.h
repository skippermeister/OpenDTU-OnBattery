// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)

#pragma once

#include <Arduino.h>
#include <driver/spi_master.h>
#include <esp_eth.h>
#include <esp_netif.h>

#include <memory>

class W5500Class
{
private:
    explicit W5500Class(spi_device_handle_t spi, gpio_num_t pin_int);

public:
    W5500Class(const W5500Class&) = delete;
    W5500Class& operator=(const W5500Class&) = delete;
    ~W5500Class();

    static std::unique_ptr<W5500> setup(int8_t pin_mosi, int8_t pin_miso, int8_t pin_sclk, int8_t pin_cs, int8_t pin_int, int8_t pin_rst);
    String macAddress();

private:
    static bool connection_check_spi(spi_device_handle_t spi);
    static bool connection_check_interrupt(gpio_num_t pin_int);

    esp_eth_handle_t eth_handle;
    esp_netif_t* eth_netif;
};

#endif
