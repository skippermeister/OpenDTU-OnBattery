// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <optional>
#include <string>

class SPIPortManagerClass {
public:
    void init();

    std::optional<uint8_t> allocatePort(std::string const& owner);
    void freePort(std::string const& owner);

    // the amount of SPIs available on supported ESP32 chips
    #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    static size_t constexpr _num_controllers = 2;
    #else
    static size_t constexpr _num_controllers = 1;
    #endif
    #if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
    static int8_t constexpr _start_spi_num = 0;
    static int8_t constexpr _offset_spi_num = 1;
    #else
    static int8_t constexpr _start_spi_num = 2;
    static int8_t constexpr  _offset_spi_num = -1;
    #endif

private:

    std::array<std::string, _num_controllers> _ports = { "" };
};

extern SPIPortManagerClass SPIPortManager;
