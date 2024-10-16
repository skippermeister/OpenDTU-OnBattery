// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 - 2023 Thomas Basler and others
 */
#include "PinMapping.h"
#include "MessageOutput.h"
#include "Configuration.h"
#include "PowerMeter.h"
#include "Display_Graphic.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE 0  // DisplayType_t::SSD1309  entspricht 5
#endif

#ifndef DISPLAY_DATA
#define DISPLAY_DATA -1
#endif

#ifndef DISPLAY_CLK
#define DISPLAY_CLK -1
#endif

#ifndef DISPLAY_CS
#define DISPLAY_CS -1
#endif

#ifndef DISPLAY_RESET
#define DISPLAY_RESET -1
#endif

#ifndef DISPLAY_BUSY
#define DISPLAY_BUSY 255
#endif

#ifndef DISPLAY_DC
#define DISPLAY_DC 255
#endif

#ifndef LED0
#define LED0 -1
#endif

#ifndef LED1
#define LED1 -1
#endif

#ifndef LED_RGB
#define LED_RGB -1
#endif

#ifndef NRF24_PIN_SCLK
#define NRF24_PIN_SCLK -1
#endif

#ifndef NRF24_PIN_CS
#define NRF24_PIN_CS -1
#endif

#ifndef NRF24_PIN_CE
#define NRF24_PIN_CE -1
#endif

#ifndef NRF24_PIN_IRQ
#define NRF24_PIN_IRQ -1
#endif

#ifndef NRF24_PIN_MISO
#define NRF24_PIN_MISO -1
#endif

#ifndef NRF24_PIN_MOSI
#define NRF24_PIN_MOSI -1
#endif

#ifndef CMT_CLK
#define CMT_CLK -1
#endif

#ifndef CMT_CS
#define CMT_CS -1
#endif

#ifndef CMT_FCS
#define CMT_FCS -1
#endif

#ifndef CMT_GPIO2
#define CMT_GPIO2 -1
#endif

#ifndef CMT_GPIO3
#define CMT_GPIO3 -1
#endif

#ifndef CMT_SDIO
#define CMT_SDIO -1
#endif

#ifndef VICTRON_PIN_RX
#define VICTRON_PIN_RX -1
#endif

#ifndef VICTRON_PIN_TX
#define VICTRON_PIN_TX -1
#endif

#ifndef REFUSOL_PIN_RX
#define REFUSOL_PIN_RX -1
#endif

#ifndef REFUSOL_PIN_TX
#define REFUSOL_PIN_TX -1
#endif

#ifndef REFUSOL_PIN_RTS
#define REFUSOL_PIN_RTS -1
#endif

#ifndef BATTERY_PIN_RX
#define BATTERY_PIN_RX -1
#endif

#ifndef BATTERY_PIN_TX
#define BATTERY_PIN_TX -1
#endif

#ifndef BATTERY_PIN_RTS
#define BATTERY_PIN_RTS -1
#endif

#ifndef BATTERY_PIN_WAKEUP
#define BATTERY_PIN_WAKEUP -1
#endif

#ifndef I2C_PIN_SCL
#define I2C_PIN_SCL -1
#endif

#ifndef I2C_PIN_SDA
#define I2C_PIN_SDA -1
#endif

#ifndef CAN0_PIN_RX
#define CAN0_PIN_RX -1
#endif

#ifndef CAN0_PIN_TX
#define CAN0_PIN_TX -1
#endif

#ifndef MCP2515_PIN_MISO
#define MCP2515_PIN_MISO -1
#endif

#ifndef MCP2515_PIN_MOSI
#define MCP2515_PIN_MOSI -1
#endif

#ifndef MCP2515_PIN_SCLK
#define MCP2515_PIN_SCLK -1
#endif

#ifndef MCP2515_PIN_IRQ
#define MCP2515_PIN_IRQ -1
#endif

#ifndef MCP2515_PIN_CS
#define MCP2515_PIN_CS -1
#endif

#ifndef PRE_CHARGE_PIN
#define PRE_CHARGE_PIN -1
#endif

#ifndef FULL_POWER_PIN
#define FULL_POWER_PIN -1
#endif

#ifndef POWERMETER_PIN_RX
#define POWERMETER_PIN_RX -1
#endif

#ifndef POWERMETER_PIN_TX
#define POWERMETER_PIN_TX -1
#endif

#ifndef POWERMETER_PIN_RTS
#define POWERMETER_PIN_RTS -1
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)
#ifndef W5500_SCLK
#define W5500_SCLK -1
#endif

#ifndef W5500_MOSI
#define W5500_MOSI -1
#endif

#ifndef W5500_MISO
#define W5500_MISO -1
#endif

#ifndef W5500_CS
#define W5500_CS -1
#endif

#ifndef W5500_IRQ
#define W5500_IRQ -1
#endif

#ifndef W5500_RST
#define W5500_RST -1
#endif
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_EMAC)

#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR -1
#endif

#ifndef ETH_PHY_POWER
#define ETH_PHY_POWER -1
#endif

#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC -1
#endif

#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO -1
#endif

#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#endif

#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#endif

#endif // CONFIG_ETH_USE_ESP32_EMAC

#ifndef CHARGER_PIN_POWER
#define CHARGER_PIN_POWER -1
#endif

PinMappingClass PinMapping;

PinMappingClass::PinMappingClass()
{
    memset(&_pinMapping, 0x0, sizeof(_pinMapping));
    _pinMapping.nrf24_clk = NRF24_PIN_SCLK;
    _pinMapping.nrf24_cs = NRF24_PIN_CS;
    _pinMapping.nrf24_en = NRF24_PIN_CE;
    _pinMapping.nrf24_irq = NRF24_PIN_IRQ;
    _pinMapping.nrf24_miso = NRF24_PIN_MISO;
    _pinMapping.nrf24_mosi = NRF24_PIN_MOSI;

#if defined(USE_RADIO_CMT)
    _pinMapping.cmt_clk = CMT_CLK;
    _pinMapping.cmt_cs = CMT_CS;
    _pinMapping.cmt_fcs = CMT_FCS;
    _pinMapping.cmt_gpio2 = CMT_GPIO2;
    _pinMapping.cmt_gpio3 = CMT_GPIO3;
    _pinMapping.cmt_sdio = CMT_SDIO;
    _pinMapping.cmt_chip_int1gpio = 2;
    _pinMapping.cmt_chip_int2gpio = 3;
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)
    _pinMapping.w5500.sclk = W5500_SCLK;
    _pinMapping.w5500.mosi = W5500_MOSI;
    _pinMapping.w5500.miso = W5500_MISO;
    _pinMapping.w5500.cs = W5500_CS;
    _pinMapping.w5500.irq = W5500_IRQ;
    _pinMapping.w5500.rst = W5500_RST;
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_EMAC)
    _pinMapping.eth.enabled = true;
    _pinMapping.eth.phy_addr = ETH_PHY_ADDR;
    _pinMapping.eth.power = ETH_PHY_POWER;
    _pinMapping.eth.mdc = ETH_PHY_MDC;
    _pinMapping.eth.mdio = ETH_PHY_MDIO;
    _pinMapping.eth.type = ETH_PHY_TYPE;
    _pinMapping.eth.clk_mode = ETH_CLK_MODE;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    _pinMapping.display_type = DISPLAY_TYPE;
    _pinMapping.display_data = DISPLAY_DATA;
    _pinMapping.display_clk = DISPLAY_CLK;
    _pinMapping.display_cs = DISPLAY_CS;
    _pinMapping.display_reset = DISPLAY_RESET;
    _pinMapping.display_busy = DISPLAY_BUSY;
    _pinMapping.display_dc = DISPLAY_DC;
#endif

#if defined(USE_LED_SINGLE)
    _pinMapping.led[0] = LED0;
    _pinMapping.led[1] = LED1;
#endif
#if defined(USE_LED_STRIP)
    _pinMapping.led_rgb = LED_RGB;
#endif

    _pinMapping.victron[0].tx = VICTRON_PIN_TX;
    _pinMapping.victron[0].rx = VICTRON_PIN_RX;

    _pinMapping.victron[1].tx = -1;
    _pinMapping.victron[1].rx = -1;

    _pinMapping.victron[2].tx = -1;
    _pinMapping.victron[2].rx = -1;

#if defined(USE_REFUsol_INVERTER)
    _pinMapping.REFUsol.rx = REFUSOL_PIN_RX;
    _pinMapping.REFUsol.tx = REFUSOL_PIN_TX;
    _pinMapping.REFUsol.rts = REFUSOL_PIN_RTS;
#endif

#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_GOBEL_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)  || defined(USE_JBDBMS_CONTROLLER)
    _pinMapping.battery.provider = Battery_Provider_t::RS485;
    _pinMapping.battery.rs485.rx = BATTERY_PIN_RX;
    _pinMapping.battery.rs485.tx = BATTERY_PIN_TX;
    _pinMapping.battery.rs485.rts = BATTERY_PIN_RTS;
#if defined(USE_DALYBMS_CONTROLLER)
    _pinMapping.battery.wakeup = BATTERY_PIN_WAKEUP;
#endif
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)
    _pinMapping.battery.provider = Battery_Provider_t::CAN0;
    _pinMapping.battery.can0.rx = CAN0_PIN_RX;
    _pinMapping.battery.can0.tx = CAN0_PIN_TX;
#endif
    _pinMapping.battery.providerName = help[static_cast<int>(_pinMapping.battery.provider)];

#if defined(USE_CHARGER_HUAWEI)
    _pinMapping.charger.power = CHARGER_PIN_POWER;
#endif
    _pinMapping.charger.provider = Charger_Provider_t::CAN0;
    _pinMapping.charger.can0.rx = CAN0_PIN_RX;
    _pinMapping.charger.can0.tx = CAN0_PIN_TX;
    _pinMapping.charger.providerName = help[static_cast<int>(_pinMapping.charger.provider)];

    _pinMapping.pre_charge = PRE_CHARGE_PIN;
    _pinMapping.full_power = FULL_POWER_PIN;

    _pinMapping.powermeter_tx = POWERMETER_PIN_TX;
    _pinMapping.powermeter_rx = POWERMETER_PIN_RX;
    _pinMapping.powermeter_rts = POWERMETER_PIN_RTS;
}

PinMapping_t& PinMappingClass::get()
{
    return _pinMapping;
}

void PinMappingClass::init(const String& deviceMapping)
{
    MessageOutput.print("Reading PinMapping... ");

    File f = LittleFS.open(PINMAPPING_FILENAME, "r", false);

    if (f) {

        JsonDocument doc;
        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, f);
        if (error) {
            MessageOutput.println("Failed to read file, using default configuration");
            return;
        }

        for (uint8_t i = 0; i < doc.size(); i++) {
            String devName = doc[i]["name"] | "";
            if (devName == deviceMapping) {
                MessageOutput.printf("found valid mapping %s ", devName.c_str());

#if defined(USE_RADIO_NRF)
                strlcpy(_pinMapping.name, devName.c_str(), sizeof(_pinMapping.name));
                _pinMapping.nrf24_clk = doc[i]["nrf24"]["clk"] | NRF24_PIN_SCLK;
                _pinMapping.nrf24_cs = doc[i]["nrf24"]["cs"] | NRF24_PIN_CS;
                _pinMapping.nrf24_en = doc[i]["nrf24"]["en"] | NRF24_PIN_CE;
                _pinMapping.nrf24_irq = doc[i]["nrf24"]["irq"] | NRF24_PIN_IRQ;
                _pinMapping.nrf24_miso = doc[i]["nrf24"]["miso"] | NRF24_PIN_MISO;
                _pinMapping.nrf24_mosi = doc[i]["nrf24"]["mosi"] | NRF24_PIN_MOSI;
#endif

#if defined(USE_RADIO_CMT)
                _pinMapping.cmt_clk = doc[i]["cmt"]["clk"] | CMT_CLK;
                _pinMapping.cmt_cs = doc[i]["cmt"]["cs"] | CMT_CS;
                _pinMapping.cmt_fcs = doc[i]["cmt"]["fcs"] | CMT_FCS;
                _pinMapping.cmt_gpio2 = doc[i]["cmt"]["gpio2"] | CMT_GPIO2;
                _pinMapping.cmt_gpio3 = doc[i]["cmt"]["gpio3"] | CMT_GPIO3;
                _pinMapping.cmt_sdio = doc[i]["cmt"]["sdio"] | CMT_SDIO;

                _pinMapping.cmt_chip_int1gpio = doc[i]["cmt"]["chip_int1gpio"] | 2;
                _pinMapping.cmt_chip_int2gpio = doc[i]["cmt"]["chip_int2gpio"] | 3;
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)
                _pinMapping.w5500.sclk = doc[i]["w5500"]["sclk"] | W5500_SCLK;
                _pinMapping.w5500.mosi = doc[i]["w5500"]["mosi"] | W5500_MOSI;
                _pinMapping.w5500.miso = doc[i]["w5500"]["miso"] | W5500_MISO;
                _pinMapping.w5500.cs = doc[i]["w5500"]["cs"] | W5500_CS;
                _pinMapping.w5500.irq = doc[i]["w5500"]["irq"] | W5500_IRQ;
                _pinMapping.w5500.rst = doc[i]["w5500"]["rst"] | W5500_RST;
#endif

#if  defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_EMAC)
                _pinMapping.eth.enabled = doc[i]["eth"]["enabled"] | true;
                _pinMapping.eth.phy_addr = doc[i]["eth"]["phy_addr"] | ETH_PHY_ADDR;
                _pinMapping.eth.power = doc[i]["eth"]["power"] | ETH_PHY_POWER;
                _pinMapping.eth.mdc = doc[i]["eth"]["mdc"] | ETH_PHY_MDC;
                _pinMapping.eth.mdio = doc[i]["eth"]["mdio"] | ETH_PHY_MDIO;
                _pinMapping.eth.type = doc[i]["eth"]["type"] | ETH_PHY_TYPE;
                _pinMapping.eth.clk_mode = doc[i]["eth"]["clk_mode"] | ETH_CLK_MODE;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
                _pinMapping.display_type = doc[i]["display"]["type"] | DISPLAY_TYPE;
                _pinMapping.display_data = doc[i]["display"]["data"] | DISPLAY_DATA;
                _pinMapping.display_clk = doc[i]["display"]["clk"] | DISPLAY_CLK;
                _pinMapping.display_cs = doc[i]["display"]["cs"] | DISPLAY_CS;
                _pinMapping.display_reset = doc[i]["display"]["reset"] | DISPLAY_RESET;
                _pinMapping.display_busy = doc[i]["display"]["busy"] | DISPLAY_BUSY;
                _pinMapping.display_dc = doc[i]["display"]["dc"] | DISPLAY_DC;
#endif

#if defined(USE_LED_SINGLE)
                _pinMapping.led[0] = doc[i]["led"]["led0"] | LED0;
                _pinMapping.led[1] = doc[i]["led"]["led1"] | LED1;
#endif
#if defined(USE_LED_STRIP)
                _pinMapping.led_rgb = doc[i]["led"]["rgb"] | LED_RGB;
#endif
                for (int i=0; i<sizeof(_pinMapping.victron)/sizeof(RS232_t); i++) {
                    String offset = (i>0)?String(i+1):String("");
                    _pinMapping.victron[i].rx = doc[i]["victron"][String("rs232_rx")+offset] | -1;
                    _pinMapping.victron[i].tx = doc[i]["victron"][String("rs232_tx")+offset] | -1;
                }

#if defined(USE_REFUsol_INVERTER)
                _pinMapping.REFUsol.rx = doc[i]["refusol"]["rs485_rx"] | REFUSOL_PIN_RX;
                _pinMapping.REFUsol.tx = doc[i]["refusol"]["rs485_tx"] | REFUSOL_PIN_TX;
                if (doc[i]["refusol"]["rs485_rts"].is<int8_t>()) {
                    _pinMapping.REFUsol.rts = doc[i]["refusol"]["rs485_rts"] | REFUSOL_PIN_RTS;
                } else {
                    _pinMapping.REFUsol.rts = -1;
                }
#endif

#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_GOBEL_RS485_RECEIVER)|| defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKMS_CONTROLLER)
                if (doc[i]["battery"]["rs485_rx"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::RS485;
                    _pinMapping.battery.rs485.rx = doc[i]["battery"]["rs485_rx"] | BATTERY_PIN_RX;
                    _pinMapping.battery.rs485.tx = doc[i]["battery"]["rs485_tx"] | BATTERY_PIN_TX;
                    if (doc[i]["battery"]["rs485_rts"].is<int8_t>()) {
                        _pinMapping.battery.rs485.rts = doc[i]["battery"]["rs485_rts"] | BATTERY_PIN_RTS;
                    } else {
                        _pinMapping.battery.rs485.rts = -1;
                    }
#if defined(USE_DALYBMS_CONTROLLER)
                    _pinMapping.battery.wakeup = doc[i]["battery"]["wakeup"] | BATTERY_PIN_WAKEUP;
#endif
                } else
#endif
#if defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
                if (doc[i]["battery"]["rs232_rx"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::RS232;
                    _pinMapping.battery.rs232.rx = doc[i]["battery"]["rs232_rx"] | BATTERY_PIN_RX;
                    _pinMapping.battery.rs232.tx = doc[i]["battery"]["rs232_tx"] | BATTERY_PIN_TX;
#if defined(USE_DALYBMS_CONTROLLER)
                    _pinMapping.battery.wakeup = doc[i]["battery"]["wakeup"] | BATTERY_PIN_WAKEUP;
#endif
                } else
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER) || defined(USE_SBS_CAN_RECEIVER)
#ifdef USE_BATTERY_CAN0
                if (doc[i]["battery"]["can0_rx"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::CAN0;
                    _pinMapping.battery.can0.rx = doc[i]["battery"]["can0_rx"] | -1;
                    _pinMapping.battery.can0.tx = doc[i]["battery"]["can0_tx"] | -1;
                } else
#endif
#ifdef USE_BATTERY_I2C
                 if (doc[i]["battery"]["i2c0_scl"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::I2C0;
                    _pinMapping.battery.i2c.scl = doc[i]["battery"]["i2c0_scl"] | -1;
                    _pinMapping.battery.i2c.sda = doc[i]["battery"]["i2c0_sda"] | -1;
                } else if (doc[i]["battery"]["i2c1_scl"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::I2C1;
                    _pinMapping.battery.i2c.scl = doc[i]["battery"]["i2c1_scl"] | -1;
                    _pinMapping.battery.i2c.sda = doc[i]["battery"]["i2c1_sda"] | -1;
                } else
#endif
#ifdef USE_BATTERY_MCP2515
                 if (doc[i]["battery"]["mcp2515_miso"].is<int8_t>()) {
                    _pinMapping.battery.provider = Battery_Provider_t::MCP2515;
                    _pinMapping.battery.mcp2515.miso = doc[i]["battery"]["mcp2515_miso"] | -1;
                    _pinMapping.battery.mcp2515.mosi = doc[i]["battery"]["mcp2515_mosi"] | -1;
                    _pinMapping.battery.mcp2515.clk = doc[i]["battery"]["mcp2515_clk"] | -1;
                    _pinMapping.battery.mcp2515.irq = doc[i]["battery"]["mcp2515_irq"] | -1;
                    _pinMapping.battery.mcp2515.cs = doc[i]["battery"]["mcp2515_cs"] | -1;
                } else
#endif
#endif
                {
                    _pinMapping.battery.provider = Battery_Provider_t::RS485;
                    _pinMapping.battery.rs485.rx = BATTERY_PIN_RX;
                    _pinMapping.battery.rs485.tx = BATTERY_PIN_TX;
                    _pinMapping.battery.rs485.rts = BATTERY_PIN_RTS;
#if defined(USE_DALYBMS_CONTROLLER)
                    _pinMapping.battery.wakeup = doc[i]["battery"]["wakeup"] | BATTERY_PIN_WAKEUP;
#endif
                }
                _pinMapping.battery.providerName = help[static_cast<int>(_pinMapping.battery.provider)];

#if defined(USE_CHARGER_MEANWELL) || defined(USE_CHARGER_HUAWEI)
#if defined(USE_CHARGER_HUAWEI)
                _pinMapping.charger.power = doc[i]["charger"]["power"] | CHARGER_PIN_POWER;
#endif

#ifdef USE_CHARGER_CAN0
                if (doc[i]["charger"]["can0_rx"].is<int8_t>()) {
                    _pinMapping.charger.provider = Charger_Provider_t::CAN0;
                    _pinMapping.charger.can0.rx = doc[i]["charger"]["can0_rx"] | -1;
                    _pinMapping.charger.can0.tx = doc[i]["charger"]["can0_tx"] | -1;
                } else
#endif
#ifdef USE_CHARGER_I2C
                 if (doc[i]["charger"]["i2c0_scl"].is<int8_t>()) {
                    _pinMapping.charger.provider = Charger_Provider_t::I2C0;
                    _pinMapping.charger.i2c.scl = doc[i]["charger"]["i2c0_scl"] | -1;
                    _pinMapping.charger.i2c.sda = doc[i]["charger"]["i2c0_sda"] | -1;
                } else if (doc[i]["charger"]["i2c1_scl"].is<int8_t>()) {
                    _pinMapping.charger.provider = Charger_Provider_t::I2C1;
                    _pinMapping.charger.i2c.scl = doc[i]["charger"]["i2c1_scl"] | -1;
                    _pinMapping.charger.i2c.sda = doc[i]["charger"]["i2c1_sda"] | -1;
                } else
#endif
#ifdef USE_CHARGER_MCP2515
                 if (doc[i]["charger"]["mcp2515_miso"].is<int8_t>()) {
                    _pinMapping.charger.provider = Charger_Provider_t::MCP2515;
                    _pinMapping.charger.mcp2515.miso = doc[i]["charger"]["mcp2515_miso"] | -1;
                    _pinMapping.charger.mcp2515.mosi = doc[i]["charger"]["mcp2515_mosi"] | -1;
                    _pinMapping.charger.mcp2515.clk = doc[i]["charger"]["mcp2515_clk"] | -1;
                    _pinMapping.charger.mcp2515.irq = doc[i]["charger"]["mcp2515_irq"] | -1;
                    _pinMapping.charger.mcp2515.cs = doc[i]["charger"]["mcp2515_cs"] | -1;
                } else
#endif
                {
                    // default to CAN0
                    _pinMapping.charger.provider = Charger_Provider_t::CAN0;
                    _pinMapping.charger.can0.rx = doc[i]["charger"]["can0_rx"] | CAN0_PIN_RX;
                    _pinMapping.charger.can0.tx = doc[i]["charger"]["can0_tx"] | CAN0_PIN_TX;
                }
#endif
                _pinMapping.charger.providerName = help[static_cast<int>(_pinMapping.charger.provider)];

                _pinMapping.pre_charge = doc[i]["batteryConnectedInverter"]["pre_charge"] | PRE_CHARGE_PIN;
                _pinMapping.full_power = doc[i]["batteryConnectedInverter"]["full_power"] | FULL_POWER_PIN;

                if (doc[i]["powermeter"]["sml_rs232_rx"].is<int8_t>()) { // SML Interface
                    _pinMapping.powermeter_rx = doc[i]["powermeter"]["sml_rs232_rx"] | POWERMETER_PIN_RX;
                    _pinMapping.powermeter_tx = doc[i]["powermeter"]["sml_rs232_tx"] | POWERMETER_PIN_TX;
                    _pinMapping.powermeter_rts = -1;
                } else if (doc[i]["powermeter"]["sdm_rs485_rx"].is<int8_t>()) {  // SDM Interface
                    _pinMapping.powermeter_rx = doc[i]["powermeter"]["sdm_rs485_rx"] | POWERMETER_PIN_RX;
                    _pinMapping.powermeter_tx = doc[i]["powermeter"]["sdm_rs485_tx"] | POWERMETER_PIN_TX;
                    if (doc[i]["powermeter"]["sdm_rs485_rts"].is<int8_t>()) {
                        _pinMapping.powermeter_rts = doc[i]["powermeter"]["sdm_rs485_rts"] | POWERMETER_PIN_RTS;    // Type 1 RS485 adapter
                    } else {
                        _pinMapping.powermeter_rts = -1;    // Type 2 RS485 adapter
                    }
                } else {
                    _pinMapping.powermeter_rx = POWERMETER_PIN_RX;
                    _pinMapping.powermeter_tx = POWERMETER_PIN_TX;
                    _pinMapping.powermeter_rts = POWERMETER_PIN_RTS;
                }

//    createPinMappingJson();

                MessageOutput.println("... done");
                return;
            }
        }
    }

//    createPinMappingJson();

    MessageOutput.println("using default config");
}

#if defined(USE_RADIO_NRF)
bool PinMappingClass::isValidNrf24Config() const
{
    return _pinMapping.nrf24_clk >= 0
        && _pinMapping.nrf24_cs >= 0
        && _pinMapping.nrf24_en >= 0
        && _pinMapping.nrf24_irq >= 0
        && _pinMapping.nrf24_miso >= 0
        && _pinMapping.nrf24_mosi >= 0;
}
#endif

#if defined(USE_RADIO_CMT)
bool PinMappingClass::isValidCmt2300Config() const
{
    return _pinMapping.cmt_clk > 0
        && _pinMapping.cmt_cs > 0
        && _pinMapping.cmt_fcs > 0
        && _pinMapping.cmt_sdio > 0
        && ((_pinMapping.cmt_gpio2 > 0 &&
             _pinMapping.cmt_gpio3 > 0) ||
            (_pinMapping.cmt_gpio2 < 0 &&
             _pinMapping.cmt_gpio3 < 0));
}
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)
bool PinMappingClass::isValidW5500Config() const
{
    return _pinMapping.w5500.sclk >= 0
        && _pinMapping.w5500.mosi >= 0
        && _pinMapping.w5500.miso >= 0
        && _pinMapping.w5500.cs >= 0
        && _pinMapping.w5500.irq >= 0
        && _pinMapping.w5500.rst >= 0;
}
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_EMAC)
bool PinMappingClass::isValidEthConfig() const
{
    return _pinMapping.eth.enabled
        && _pinMapping.eth.mdc >= 0
        && _pinMapping.eth.mdio >= 0;
}
#endif

bool PinMappingClass::isValidBatteryConfig() const
{
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_GOBEL_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)  || defined(USE_JBDBMS_CONTROLLER)
    if (_pinMapping.battery.provider == Battery_Provider_t::RS485)
        return    _pinMapping.battery.rs485.rx > 0
               && _pinMapping.battery.rs485.tx >= 0
               && _pinMapping.battery.rs485.rx != _pinMapping.battery.rs485.tx
               && _pinMapping.battery.rs485.rts != _pinMapping.battery.rs485.rx
               && _pinMapping.battery.rs485.rts != _pinMapping.battery.rs485.tx
#if defined (USE_DALYBMS_CONTROLLER)
               && _pinMapping.battery.wakeup != _pinMapping.battery.rs485.rx
               && _pinMapping.battery.wakeup != _pinMapping.battery.rs485.tx
#endif
           ;
    if (_pinMapping.battery.provider == Battery_Provider_t::RS232)
        return    _pinMapping.battery.rs232.rx > 0
               && _pinMapping.battery.rs232.tx >= 0
               && _pinMapping.battery.rs232.rx != _pinMapping.battery.rs232.tx
#if defined (USE_DALYBMS_CONTROLLER)
               && _pinMapping.battery.wakeup != _pinMapping.battery.rs232.rx
               && _pinMapping.battery.wakeup != _pinMapping.battery.rs232.tx
#endif
           ;
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER) || defined(USE_SBS_CAN_RECEIVER)
#ifdef USE_BATTERY_CAN0
    if (_pinMapping.battery.provider == Battery_Provider_t::CAN0)
        return    _pinMapping.battery.can0.rx > 0
               && _pinMapping.battery.can0.tx > 0
               && _pinMapping.battery.can0.rx != _pinMapping.battery.can0.tx;
#endif
#ifdef USE_BATTERY_I2C
    if ((_pinMapping.battery.provider == Battery_Provider_t::I2C0) ||
        (_pinMapping.battery.provider == Battery_Provider_t::I2C1))
        return    _pinMapping.battery.i2c.scl > 0
               && _pinMapping.battery.i2c.sda > 0
               && _pinMapping.battery.i2c.scl != _pinMapping.battery.i2c.sda;
#endif
#ifdef USE_CHARGER_MCP2515
    if (_pinMapping.battery.provider == Battery_Provider_t::MCP2515)
        return _pinMapping.battery.mcp2515.miso > 0 &&
               _pinMapping.battery.mcp2515.mosi > 0 &&
               _pinMapping.battery.mcp2515.clk > 0 &&
               _pinMapping.battery.mcp2515.irq > 0 &&
               _pinMapping.battery.mcp2515.cs > 0;
#endif
#endif
    return false;
}

bool PinMappingClass::isValidChargerConfig() const
{
#if defined(USE_CHARGER_MEANWELL) || defined(USE_CHARGER_HUAWEI)
#ifdef USE_CHARGER_CAN0
    if (_pinMapping.charger.provider == Charger_Provider_t::CAN0)
        return _pinMapping.charger.can0.rx >= 0 &&
               _pinMapping.charger.can0.tx >= 0;
#endif
#ifdef USE_CHARGER_I2C
    if ((_pinMapping.charger.provider == Charger_Provider_t::I2C0) ||
        (_pinMapping.battery.provider == Battery_Provider_t::I2C1))
        return _pinMapping.charger.i2c.scl >= 0 &&
               _pinMapping.charger.i2c.sda >= 0;
#endif
#ifdef USE_CHARGER_MCP2515
    if (_pinMapping.charger.provider == Charger_Provider_t::MCP2515)
        return _pinMapping.charger.mcp2515.miso >= 0 &&
               _pinMapping.charger.mcp2515.mosi >= 0 &&
               _pinMapping.charger.mcp2515.clk >= 0 &&
               _pinMapping.charger.mcp2515.irq >= 0 &&
               _pinMapping.charger.mcp2515.cs >= 0;
#endif
#endif
    return false;
}

bool PinMappingClass::isValidPreChargeConfig() const
{
    return _pinMapping.pre_charge >= 0
        && _pinMapping.full_power >= 0;
}

#if defined(USE_REFUsol_INVERTER)
bool PinMappingClass::isValidREFUsolConfig() const
{
    return _pinMapping.REFUsol.rx > 0
        && _pinMapping.REFUsol.tx >= 0
        && _pinMapping.REFUsol.rx != _pinMapping.REFUsol.tx
        && _pinMapping.REFUsol.rts != _pinMapping.REFUsol.rx
        && _pinMapping.REFUsol.rts != _pinMapping.REFUsol.tx;
}
#endif

void PinMappingClass::createPinMappingJson() const
{
    JsonDocument obj;

    JsonArray array = obj[""].to<JsonArray>();
    JsonObject doc = array.add<JsonObject>();

    doc["name"] = "my_very_special_board";

#if defined(USE_RADIO_NRF)
    JsonObject nrf24 = doc["nrf24"].to<JsonObject>();
    nrf24["clk"]  = _pinMapping.nrf24_clk;
    nrf24["cs"]   = _pinMapping.nrf24_cs;
    nrf24["en"]   = _pinMapping.nrf24_en;
    nrf24["irq"]  = _pinMapping.nrf24_irq;
    nrf24["miso"] = _pinMapping.nrf24_miso;
    nrf24["mosi"] = _pinMapping.nrf24_mosi;
#endif

#if defined(USE_RADIO_CMT)
    JsonObject cmt = doc["cmt"].to<JsonObject>();
    cmt["clk"]   = _pinMapping.cmt_clk;
    cmt["cs"]    = _pinMapping.cmt_cs;
    cmt["fcs"]   = _pinMapping.cmt_fcs;
    cmt["gpio2"] = _pinMapping.cmt_gpio2;
    cmt["gpio3"] = _pinMapping.cmt_gpio3;
    cmt["sdio"]  = _pinMapping.cmt_sdio;
    cmt["chip_int1gpio"] = _pinMapping.cmt_chip_int1gpio;
    cmt["chip_int2gpio"] = _pinMapping.cmt_chip_int2gpio;
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_W5500)
    auto w5500PinObj = doc["w5500"].to<JsonObject>();
    w5500PinObj["sclk"] = pin.w5500.sclk;
    w5500PinObj["mosi"] = pin.w5500.mosi;
    w5500PinObj["miso"] = pin.w5500.miso;
    w5500PinObj["cs"] = pin.w5500.cs;
    w5500PinObj["irq"] = pin.w5500.irq;
    w5500PinObj["rst"] = pin.w5500.rst;
#endif

#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(USE_EMAC)
    JsonObject eth = doc["eth"].to<JsonObject>();
    eth["enabled"]  = _pinMapping.eth.enabled;
    eth["phy_addr"] = _pinMapping.eth.phy_addr;
    eth["power"]    = _pinMapping.eth.power;
    eth["mdc"]      = _pinMapping.eth.mdc;
    eth["mdio"]     = _pinMapping.eth.mdio;
    eth["type"]     = _pinMapping.eth.type;
    eth["clk_mode"] = _pinMapping.eth.clk_mode;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    JsonObject display = doc["display"].to<JsonObject>();
    display["type"]  = _pinMapping.display_type;
    display["data"]  = _pinMapping.display_data;
    display["clk"]   = _pinMapping.display_clk;
    display["cs"]    = _pinMapping.display_cs;
    display["reset"] = _pinMapping.display_reset;
    display["busy"]  = _pinMapping.display_busy;
    display["dc"]    = _pinMapping.display_dc;
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    JsonObject led = doc["led"].to<JsonObject>();
#if defined(USE_LED_SINGLE)
    led["led0"] = _pinMapping.led[0];
    led["led1"] = _pinMapping.led[1];
#endif
#if defined(USE_LED_STRIP)
    led["rgb"] = _pinMapping.led_rgb;
#endif
#endif

    JsonObject victron = doc["victron"].to<JsonObject>();
    for (int i=0; i<sizeof(_pinMapping.victron)/sizeof(RS232_t); i++) {
        String offset = (i>0)?String(i+1):String("");
        victron[String("rs232_rx")+offset] = _pinMapping.victron[i].rx;
        victron[String("rs232_tx")+offset] = _pinMapping.victron[i].tx;
    }

#if defined(USE_REFUsol_INVERTER)
    JsonObject refusol = doc["refusol"].to<JsonObject>();
    refusol["rs485_rx"] = _pinMapping.REFUsol.rx;
    refusol["rs485_tx"] = _pinMapping.REFUsol.tx;
    if (_pinMapping.REFUsol.rts >= 0) refusol["rs485_rts"] = _pinMapping.REFUsol.rts;
#endif

    JsonObject battery = doc["battery"].to<JsonObject>();
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_GOBEL_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)  || defined(USE_JBDBMS_CONTROLLER)
    if (_pinMapping.battery.provider == Battery_Provider_t::RS485) {
        battery["rs485_rx"]  = _pinMapping.battery.rs485.rx;
        battery["rs485_tx"]  = _pinMapping.battery.rs485.tx;
        if (_pinMapping.battery.rs485.rts >= 0) battery["rs485_rts"] = _pinMapping.battery.rs485.rts;
#if defined(USE_DALYBMS_CONTROLLER)
        battery["wakeup"] = _pinMapping.battery.wakeup;
#endif
    } else if (_pinMapping.battery.provider == Battery_Provider_t::RS232) {
        battery["rs232_rx"]  = _pinMapping.battery.rs232.rx;
        battery["rs232_tx"]  = _pinMapping.battery.rs232.tx;
#if defined(USE_DALYBMS_CONTROLLER)
        battery["wakeup"] = _pinMapping.battery.wakeup;
#endif
    } else
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER) || defined(USE_SBS_CAN_RECEIVER)
    if (_pinMapping.battery.provider == Battery_Provider_t::CAN0) {
        battery["can0_rx"] = _pinMapping.battery.can0.rx;
        battery["can0_tx"] = _pinMapping.battery.can0.tx;
    } else if (_pinMapping.battery.provider == Battery_Provider_t::MCP2515) {
        battery["mcp2515_miso"] = _pinMapping.battery.mcp2515.miso;
        battery["mcp2515_mosi"] = _pinMapping.battery.mcp2515.mosi;
        battery["mcp2515_clk"] = _pinMapping.battery.mcp2515.clk;
        battery["mcp2515_irq"] = _pinMapping.battery.mcp2515.irq;
        battery["mcp2515_cs"] = _pinMapping.battery.mcp2515.cs;
    } else if (_pinMapping.battery.provider == Battery_Provider_t::I2C0) {
        battery["i2c0_scl"] = _pinMapping.battery.i2c.scl;
        battery["i2c0_sda"] = _pinMapping.battery.i2c.sda;
    } else if (_pinMapping.battery.provider == Battery_Provider_t::I2C1) {
        battery["i2c1_scl"] = _pinMapping.battery.i2c.scl;
        battery["i2c1_sda"] = _pinMapping.battery.i2c.sda;
    } else
#endif
    {}

#if defined(USE_CHARGER_MEANWELL) || defined(USE_CHARGER_HUAWEI)
    JsonObject charger = doc["charger"].to<JsonObject>();
    if (_pinMapping.charger.provider == Charger_Provider_t::CAN0) {
        charger["can0_rx"] = _pinMapping.charger.can0.rx;
        charger["can0_tx"] = _pinMapping.charger.can0.tx;
    }
    else if (_pinMapping.charger.provider == Charger_Provider_t::I2C0) {
        charger["i2c0_scl"] = _pinMapping.charger.i2c.scl;
        charger["i2c0_sda"] = _pinMapping.charger.i2c.sda;
    }
    else if (_pinMapping.charger.provider == Charger_Provider_t::I2C1) {
        charger["i2c1_scl"] = _pinMapping.charger.i2c.scl;
        charger["i2c1_sda"] = _pinMapping.charger.i2c.sda;
    }
    else if (_pinMapping.charger.provider == Charger_Provider_t::MCP2515) {
        charger["mcp2515_miso"] = _pinMapping.charger.mcp2515.miso;
        charger["mcp2515_mosi"] = _pinMapping.charger.mcp2515.mosi;
        charger["mcp2515_clk"] = _pinMapping.charger.mcp2515.clk;
        charger["mcp2515_irq"] = _pinMapping.charger.mcp2515.irq;
        charger["mcp2515_cs"] = _pinMapping.charger.mcp2515.cs;
#if defined(USE_CHARGER_HUAWEI)
        charger["power"] = _pinMapping.charger.power;
#endif
    }
#endif

    JsonObject battery_connected_inverter = doc["batteryConnectedInverter"].to<JsonObject>();
    battery_connected_inverter["pre_charge"] = _pinMapping.pre_charge;
    battery_connected_inverter["full_power"] = _pinMapping.full_power;

    PowerMeterProvider::Type source = static_cast<PowerMeterProvider::Type>(Configuration.get().PowerMeter.Source);
    JsonObject powermeter = doc["powermeter"].to<JsonObject>();
    if (source == PowerMeterProvider::Type::SERIAL_SML)
    {
        powermeter["sml_rs232_rx"] = _pinMapping.powermeter_rx;
        if (_pinMapping.powermeter_tx >= 0) powermeter["sml_rs232_tx"] = _pinMapping.powermeter_tx;
    } else if (   source == PowerMeterProvider::Type::SDM1PH
               || source == PowerMeterProvider::Type::SDM3PH)
    {
        powermeter["sdm_rs485_rx"] = _pinMapping.powermeter_rx;
        powermeter["sdm_rs485_tx"] = _pinMapping.powermeter_tx;
        if (_pinMapping.powermeter_rts >= 0) powermeter["sdm_rs485_rts"] = _pinMapping.powermeter_rts;
    }

    String buffer;
    serializeJsonPretty(array, buffer);

    Serial.println(buffer);
}
