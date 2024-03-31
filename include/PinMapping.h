// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ETH.h>
#include <stdint.h>

#define PINMAPPING_FILENAME "/pin_mapping.json"
#define PINMAPPING_LED_COUNT 2

#define MAPPING_NAME_STRLEN 31

struct PinMapping_t {
    char name[MAPPING_NAME_STRLEN + 1];

#if defined(USE_RADIO_NRF)
    int8_t nrf24_miso;
    int8_t nrf24_mosi;
    int8_t nrf24_clk;
    int8_t nrf24_irq;
    int8_t nrf24_en;
    int8_t nrf24_cs;
#endif

#if defined(USE_RADIO_CMT)
    int8_t cmt_clk;
    int8_t cmt_cs;
    int8_t cmt_fcs;
    int8_t cmt_gpio2;
    int8_t cmt_gpio3;
    int8_t cmt_sdio;
#endif

#if defined(OPENDTU_ETHERNET)
    bool eth_enabled;
    int8_t eth_phy_addr;
    int eth_power;
    int eth_mdc;
    int eth_mdio;
    eth_phy_type_t eth_type;
    eth_clock_mode_t eth_clk_mode;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    uint8_t display_type;
    int8_t display_data;
    int8_t display_clk;
    int8_t display_cs;
    int8_t display_reset;
    int8_t display_busy;
    int8_t display_dc;
#endif

#if defined(USE_LED_SINGLE)
    int8_t led[PINMAPPING_LED_COUNT];
#endif
#if defined(USE_LED_STRIP)
    int8_t led_rgb;
#endif

    int8_t victron_tx;
    int8_t victron_rx;

    int8_t victron_tx2;
    int8_t victron_rx2;

#if defined(USE_REFUsol_INVERTER)
    int8_t REFUsol_rx;
    int8_t REFUsol_tx;
    //    int8_t REFUsol_cts;
    int8_t REFUsol_rts;
#endif

    int8_t battery_rx;
    int8_t battery_tx;
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKMS_CONTROLLER)
    int8_t battery_rts;
#if defined(USE_DALYBMS_CONTROLLER)
    int8_t battery_bms_wakeup;
#endif
#endif

    int8_t mcp2515_miso;
    int8_t mcp2515_mosi;
    int8_t mcp2515_clk;
    int8_t mcp2515_irq;
    int8_t mcp2515_cs;

#if defined(CHARGER_HUAWEI)
    int8_t huawei_power;
#endif
    int8_t can0_rx;
    int8_t can0_tx;
    //    int8_t can0_stb;

    int8_t pre_charge;
    int8_t full_power;

    int8_t powermeter_tx;
    int8_t powermeter_rx;
    int8_t powermeter_rts; // DERE
};

class PinMappingClass {
public:
    PinMappingClass();
    void init(const String& deviceMapping);
    PinMapping_t& get();

#if defined(USE_RADIO_NRF)
    bool isValidNrf24Config() const;
#endif
#if defined(USE_RADIO_CMT)
    bool isValidCmt2300Config() const;
#endif
#if defined(OPENDTU_ETHERNET)
    bool isValidEthConfig() const;
#endif
    bool isValidBatteryConfig() const;
#if defined(CHARGER_HUAWEI)
    bool isValidHuaweiConfig() const;
#else
    bool isValidMeanWellConfig() const;
#endif

    bool isValidPreChargeConfig() const;

private:
    void createPinMappingJson() const;

    PinMapping_t _pinMapping;
};

extern PinMappingClass PinMapping;
