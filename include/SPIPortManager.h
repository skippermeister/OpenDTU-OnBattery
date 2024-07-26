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

private:
    // the amount of SPIs available on supported ESP32 chips
    #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    static size_t constexpr _num_controllers = 4;
    #else
    static size_t constexpr _num_controllers = 3;
    #endif
    std::array<std::string, _num_controllers> _ports = { "" };
};

extern SPIPortManagerClass SPIPortManager;
