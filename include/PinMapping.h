// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ETH.h>
#include <stdint.h>

#define PINMAPPING_FILENAME "/pin_mapping.json"
#define PINMAPPING_LED_COUNT 2

#define MAPPING_NAME_STRLEN 31

struct MCP2515_t {
        int8_t miso;
        int8_t mosi;
        int8_t clk;
        int8_t irq;
        int8_t cs;
};

enum class Charger_Provider_t { undefined=0, CAN0=1, MCP2515=2, I2C0=3, I2C1=4};
struct CHARGER_t {
    Charger_Provider_t provider;
    const char *providerName;
    union {
        struct {
            int8_t rx;
            int8_t tx;
        } can0;
        MCP2515_t mcp2515;
        struct {
            int8_t scl;
            int8_t sda;
        } i2c;
    };
#if defined(USE_CHARGER_HUAWEI)
    int8_t power;
#endif
};

enum class Battery_Provider_t { undefined=0, CAN0=1, MCP2515=2, I2C0=3, I2C1=4, RS232=5, RS485=6};
struct Battery_t {
    const char *providerName;
    Battery_Provider_t provider;
    union {
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)
        struct {
            int8_t rx;
            int8_t tx;
        } can0;
        MCP2515_t mcp2515;
        struct {
            int8_t scl;
            int8_t sda;
        } i2c;
#endif
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
        struct {
            int8_t rx;
            int8_t tx;
        } rs232;
        struct {
            int8_t rx;
            int8_t tx;
            int8_t rts;
        } rs485;
#endif
    };
#if defined(USE_DALYBMS_CONTROLLER)
    int8_t wakeup;
#endif
};

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
    int8_t cmt_chip_int1gpio;
    int8_t cmt_chip_int2gpio;
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

    int8_t victron_tx3;
    int8_t victron_rx3;

#if defined(USE_REFUsol_INVERTER)
    int8_t REFUsol_rx;
    int8_t REFUsol_tx;
    //    int8_t REFUsol_cts;
    int8_t REFUsol_rts;
#endif

    Battery_t battery;

    CHARGER_t charger;

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
#if defined(USE_CHARGER_MEANWELL) || defined(USE_CHARGER_HUAWEI)
    bool isValidChargerConfig() const;
#endif

    bool isValidPreChargeConfig() const;

private:
    void createPinMappingJson() const;

    PinMapping_t _pinMapping;

    const char* help[7] = {"unknown", "CAN0 Bus", "MCP2515", "I2C0/CAN", "I2C1/CAN", "RS232", "RS485"};
};

extern PinMappingClass PinMapping;
