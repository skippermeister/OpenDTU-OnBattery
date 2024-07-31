// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <optional>
#include <string>
#include <driver/spi_master.h>

/**
 * SPI# to SPI ID and SPI_HOST mapping
 *
 * ESP32-S3
 * | SPI # | SPI ID | SPI_HOST |
 * | 0     | 0      | 0        |
 * | 1     | 1      | 1        |
 * | 2     | 3      | 2        |
 *
 * ESP32
 * | SPI # | SPI ID | SPI_HOST |
 * | 0     | 1      | 0        |
 * | 1     | 2      | 1        |
 * | 2     | 3      | 2        |
 */

class SPIPortManagerClass {
public:
    void init();

    std::optional<uint8_t> allocatePort(std::string const& owner);
    void freePort(std::string const& owner);

    spi_host_device_t SPIhostNum(uint8_t spi_num) { return (spi_host_device_t)(spi_num + _offset_spi_num); }

private:

    // the amount of SPIs available on supported ESP32 chips
    #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    static size_t constexpr _num_controllers = 2;
    #else
    static size_t constexpr _num_controllers = 1;
    #endif
    #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
    static int8_t constexpr _start_spi_num = 0; // FSPI=0, HSPI=1
    static int8_t constexpr _offset_spi_num = 1; // SPI1_HOST=0 but not usable, SPI2_HOST=1 and SPI3_HOST=2, first usable is SPI2_HOST
    #else
    static int8_t constexpr _start_spi_num = 2; // HSPI=2, VSPI=3
    static int8_t constexpr  _offset_spi_num = -1; // SPI1_HOST=0 but not usable, SPI2_HOST=1 and SPI3_HOST=2, first usable is SPI2_HOST
    #endif

    std::array<std::string, _num_controllers> _ports = { "" };
};

extern SPIPortManagerClass SPIPortManager;
