// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 - 2023 Thomas Basler and others
 */
#include "PinMapping.h"
#include "MessageOutput.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

#define JSON_BUFFER_SIZE 6144

#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE 0U
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

#ifndef HOYMILES_PIN_SCLK
#define HOYMILES_PIN_SCLK -1
#endif

#ifndef HOYMILES_PIN_CS
#define HOYMILES_PIN_CS -1
#endif

#ifndef HOYMILES_PIN_CE
#define HOYMILES_PIN_CE -1
#endif

#ifndef HOYMILES_PIN_IRQ
#define HOYMILES_PIN_IRQ -1
#endif

#ifndef HOYMILES_PIN_MISO
#define HOYMILES_PIN_MISO -1
#endif

#ifndef HOYMILES_PIN_MOSI
#define HOYMILES_PIN_MOSI -1
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

PinMappingClass PinMapping;

PinMappingClass::PinMappingClass()
{
    memset(&_pinMapping, 0x0, sizeof(_pinMapping));
    _pinMapping.nrf24_clk = HOYMILES_PIN_SCLK;
    _pinMapping.nrf24_cs = HOYMILES_PIN_CS;
    _pinMapping.nrf24_en = HOYMILES_PIN_CE;
    _pinMapping.nrf24_irq = HOYMILES_PIN_IRQ;
    _pinMapping.nrf24_miso = HOYMILES_PIN_MISO;
    _pinMapping.nrf24_mosi = HOYMILES_PIN_MOSI;

#ifdef USE_RADIO_CMT
    _pinMapping.cmt_clk = CMT_CLK;
    _pinMapping.cmt_cs = CMT_CS;
    _pinMapping.cmt_fcs = CMT_FCS;
    _pinMapping.cmt_gpio2 = CMT_GPIO2;
    _pinMapping.cmt_gpio3 = CMT_GPIO3;
    _pinMapping.cmt_sdio = CMT_SDIO;
#endif

#ifdef OPENDTU_ETHERNET
    _pinMapping.eth_enabled = true;
    _pinMapping.eth_phy_addr = ETH_PHY_ADDR;
    _pinMapping.eth_power = ETH_PHY_POWER;
    _pinMapping.eth_mdc = ETH_PHY_MDC;
    _pinMapping.eth_mdio = ETH_PHY_MDIO;
    _pinMapping.eth_type = ETH_PHY_TYPE;
    _pinMapping.eth_clk_mode = ETH_CLK_MODE;
#endif

#ifdef USE_DISPLAY_GRAPHIC
    _pinMapping.display_type = DISPLAY_TYPE;
    _pinMapping.display_data = DISPLAY_DATA;
    _pinMapping.display_clk = DISPLAY_CLK;
    _pinMapping.display_cs = DISPLAY_CS;
    _pinMapping.display_reset = DISPLAY_RESET;
    _pinMapping.display_busy = DISPLAY_BUSY;
    _pinMapping.display_dc = DISPLAY_DC;
#endif

#ifdef USE_LED_SINGLE
    _pinMapping.led[0] = LED0;
    _pinMapping.led[1] = LED1;
#endif
#ifdef USER_LED_STRIP
    _pinMapping.led_rgb = LED_RGB;
#endif

    _pinMapping.victron_tx = VICTRON_PIN_TX;
    _pinMapping.victron_rx = VICTRON_PIN_RX;

#ifdef USE_REFUsol_INVERTER
    _pinMapping.REFUsol_rx = RS485_PIN_RX;
    _pinMapping.REFUsol_tx = RS485_PIN_TX;
    _pinMapping.REFUsol_rts = RS485_PIN_RTS;
#endif

#ifdef PYLONTECH_RS485
    _pinMapping.battery_rx = RS485_PIN_RX;
    _pinMapping.battery_tx = RS485_PIN_TX;
    _pinMapping.battery_rts = RS485_PIN_RTS;
#else
    _pinMapping.battery_rx = PYLONTECH_PIN_RX;
    _pinMapping.battery_tx = PYLONTECH_PIN_TX;
#endif

    _pinMapping.mcp2515_miso = MCP2515_PIN_MISO;
    _pinMapping.mcp2515_mosi = MCP2515_PIN_MOSI;
    _pinMapping.mcp2515_clk = MCP2515_PIN_SCLK;
    _pinMapping.mcp2515_cs = MCP2515_PIN_CS;
    _pinMapping.mcp2515_irq = MCP2515_PIN_IRQ;

#ifdef CHARGER_HUAWEI
    _pinMapping.huawei_power = HUAWEI_PIN_POWER;
#else
    //    _pinMapping.meanwell_power = MEANWELL_PIN_POWER;
    _pinMapping.can0_rx = CAN0_PIN_RX;
    _pinMapping.can0_tx = CAN0_PIN_TX;
//    _pinMapping.can0_stb = CAN0_PIN_STB;
#endif

    _pinMapping.pre_charge = PRE_CHARGE_PIN;
    _pinMapping.full_power = FULL_POWER_PIN;

    _pinMapping.sdm_tx = SDM_TX_PIN;
    _pinMapping.sdm_rx = SDM_RX_PIN;
    _pinMapping.sdm_rts = DERE_PIN;

    _pinMapping.sml_rx = SML_RX_PIN;
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

        DynamicJsonDocument doc(JSON_BUFFER_SIZE);
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

                strlcpy(_pinMapping.name, devName.c_str(), sizeof(_pinMapping.name));
                _pinMapping.nrf24_clk = doc[i]["nrf24"]["clk"] | HOYMILES_PIN_SCLK;
                _pinMapping.nrf24_cs = doc[i]["nrf24"]["cs"] | HOYMILES_PIN_CS;
                _pinMapping.nrf24_en = doc[i]["nrf24"]["en"] | HOYMILES_PIN_CE;
                _pinMapping.nrf24_irq = doc[i]["nrf24"]["irq"] | HOYMILES_PIN_IRQ;
                _pinMapping.nrf24_miso = doc[i]["nrf24"]["miso"] | HOYMILES_PIN_MISO;
                _pinMapping.nrf24_mosi = doc[i]["nrf24"]["mosi"] | HOYMILES_PIN_MOSI;

#ifdef USE_RADIO_CMT
                _pinMapping.cmt_clk = doc[i]["cmt"]["clk"] | CMT_CLK;
                _pinMapping.cmt_cs = doc[i]["cmt"]["cs"] | CMT_CS;
                _pinMapping.cmt_fcs = doc[i]["cmt"]["fcs"] | CMT_FCS;
                _pinMapping.cmt_gpio2 = doc[i]["cmt"]["gpio2"] | CMT_GPIO2;
                _pinMapping.cmt_gpio3 = doc[i]["cmt"]["gpio3"] | CMT_GPIO3;
                _pinMapping.cmt_sdio = doc[i]["cmt"]["sdio"] | CMT_SDIO;
#endif

#ifdef OPENDTU_ETHERNET
                _pinMapping.eth_enabled = doc[i]["eth"]["enabled"] | true;
                _pinMapping.eth_phy_addr = doc[i]["eth"]["phy_addr"] | ETH_PHY_ADDR;
                _pinMapping.eth_power = doc[i]["eth"]["power"] | ETH_PHY_POWER;
                _pinMapping.eth_mdc = doc[i]["eth"]["mdc"] | ETH_PHY_MDC;
                _pinMapping.eth_mdio = doc[i]["eth"]["mdio"] | ETH_PHY_MDIO;
                _pinMapping.eth_type = doc[i]["eth"]["type"] | ETH_PHY_TYPE;
                _pinMapping.eth_clk_mode = doc[i]["eth"]["clk_mode"] | ETH_CLK_MODE;
#endif

#ifdef USE_DISPLAY_GRAPHIC
                _pinMapping.display_type = doc[i]["display"]["type"] | DISPLAY_TYPE;
                _pinMapping.display_data = doc[i]["display"]["data"] | DISPLAY_DATA;
                _pinMapping.display_clk = doc[i]["display"]["clk"] | DISPLAY_CLK;
                _pinMapping.display_cs = doc[i]["display"]["cs"] | DISPLAY_CS;
                _pinMapping.display_reset = doc[i]["display"]["reset"] | DISPLAY_RESET;
                _pinMapping.display_busy = doc[i]["display"]["busy"] | DISPLAY_BUSY;
                _pinMapping.display_dc = doc[i]["display"]["dc"] | DISPLAY_DC;
#endif

#ifdef USE_LED_SINGLE
                _pinMapping.led[0] = doc[i]["led"]["led0"] | LED0;
                _pinMapping.led[1] = doc[i]["led"]["led1"] | LED1;
#endif
#ifdef USE_LED_STRIP
                _pinMapping.led_rgb = doc[i]["led"]["rgb"] | LED_RGB;
#endif
                _pinMapping.victron_rx = doc[i]["victron"]["rs232"]["rx"] | VICTRON_PIN_RX;
                _pinMapping.victron_tx = doc[i]["victron"]["rs232"]["tx"] | VICTRON_PIN_TX;

#ifdef USE_REFUsol_INVERTER
                _pinMapping.REFUsol_rx = doc[i]["refusol"]["rs485"]["rx"] | RS485_PIN_RX;
                _pinMapping.REFUsol_tx = doc[i]["refusol"]["rs485"]["tx"] | RS485_PIN_TX;
                _pinMapping.REFUsol_rts = doc[i]["refusol"]["rs485"]["rts"] | RS485_PIN_RTS;
#endif

#ifdef PYLONTECH_RS485
                _pinMapping.battery_rx = doc[i]["battery"]["rs485"]["rx"] | RS485_PIN_RX;
                _pinMapping.battery_tx = doc[i]["battery"]["rs485"]["tx"] | RS485_PIN_TX;
                _pinMapping.battery_rts = doc[i]["battery"]["rs485"]["rts"] | RS485_PIN_RTS;
#else
                _pinMapping.battery_rx = doc[i]["battery"]["can0"]["rx"] | PYLONTECH_PIN_RX;
                _pinMapping.battery_tx = doc[i]["battery"]["can0"]["tx"] | PYLONTECH_PIN_TX;
#endif

#ifdef CHARGER_HUAWEI
                _pinMapping.huawei_power = doc[i]["huawei"]["power"] | HUAWEI_PIN_POWER;
                _pinMapping.mcp2515_miso = doc[i]["huawei"]["mcp2515"]["miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["huawei"]["mcp2515"]["mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["huawei"]["mcp2515"]["clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["huawei"]["mcp2515"]["irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["huawei"]["mcp2515"]["cs"] | MCP2515_PIN_CS;
#else
                //                _pinMapping.meanwell_power = doc[i]["meanwell"]["power"] | MEANWELL_PIN_POWER;
#ifdef CHARGER_USE_CAN0
                _pinMapping.can0_rx = doc[i]["meanwell"]["can0"]["rx"] | CAN0_PIN_RX;
                _pinMapping.can0_tx = doc[i]["meanwell"]["can0"]["tx"] | CAN0_PIN_RX;
                //                _pinMapping.can0_stb = doc[i]["meanwell"]["can0"]["stb"] | CAN0_PIN_STB;

                _pinMapping.mcp2515_miso = doc[i]["mcp2515"]["miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["mcp2515"]["mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["mcp2515"]["clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["mcp2515"]["irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["mcp2515"]["cs"] | MCP2515_PIN_CS;
#else
                _pinMapping.mcp2515_miso = doc[i]["meanwell"]["mcp2515"]["miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["meanwell"]["mcp2515"]["mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["meanwell"]["mcp2515"]["clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["meanwell"]["mcp2515"]["irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["meanwell"]["mcp2515"]["cs"] | MCP2515_PIN_CS;
#endif
#endif
                _pinMapping.pre_charge = doc[i]["battery_connected_inverter"]["pre_charge"] | PRE_CHARGE_PIN;
                _pinMapping.full_power = doc[i]["battery_connected_inverter"]["full_power"] | FULL_POWER_PIN;

                _pinMapping.sdm_tx = doc[i]["sdm"]["rs485"]["tx"] | SDM_TX_PIN;
                _pinMapping.sdm_rx = doc[i]["sdm"]["rs485"]["rx"] | SDM_RX_PIN;
                _pinMapping.sdm_rts = doc[i]["sdm"]["rs485"]["rts"] | DERE_PIN;

                _pinMapping.sml_rx = doc[i]["sml"]["rs232"]["rx"] | SML_RX_PIN;

                MessageOutput.println("... done");
                return;
            }
        }
    }

    MessageOutput.println("using default config");
}

bool PinMappingClass::isValidNrf24Config() const
{
    return _pinMapping.nrf24_clk >= 0
        && _pinMapping.nrf24_cs >= 0
        && _pinMapping.nrf24_en >= 0
        && _pinMapping.nrf24_irq >= 0
        && _pinMapping.nrf24_miso >= 0
        && _pinMapping.nrf24_mosi >= 0;
}

#ifdef USE_RADIO_CMT
bool PinMappingClass::isValidCmt2300Config() const
{
    return _pinMapping.cmt_clk >= 0
        && _pinMapping.cmt_cs >= 0
        && _pinMapping.cmt_fcs >= 0
        && _pinMapping.cmt_sdio >= 0;
}
#endif

#ifdef OPENDTU_ETHERNET
bool PinMappingClass::isValidEthConfig() const
{
    return _pinMapping.eth_enabled;
}
#endif

#ifdef USE_REFUsol_INVERTER
bool PinMappingClass::isValidREFUsolConfig() const
{
    return _pinMapping.REFUsol_rx > 0
        && _pinMapping.REFUsol_tx > 0
        //        && _pinMapping.REFUsol_cts > 0
        && (_pinMapping.REFUsol_rts > 0 || _pinMapping.REFUsol_rts == -1);
}
#endif

bool PinMappingClass::isValidBatteryConfig() const
{
#ifndef PYLONTECH_RS485
    return _pinMapping.battery_rx > 0
        && _pinMapping.battery_tx > 0;
#else
    return _pinMapping.battery_rx > 0
        && _pinMapping.battery_tx > 0
        //        && _pinMapping.battery_cts > 0
        && (_pinMapping.battery_rts > 0 || _pinMapping.battery_rts == -1);
#endif
}
#ifdef CHARGER_HUAWEI
bool PinMappingClass::isValidHuaweiConfig() const
{
    return _pinMapping.mcp2515_miso > 0
        && _pinMapping.mcp2515_mosi > 0
        && _pinMapping.mcp2515_clk > 0
        && _pinMapping.mcp2515_irq > 0
        && _pinMapping.mcp2515_cs > 0
        && _pinMapping.huawei_power > 0;
}
#else
bool PinMappingClass::isValidMeanWellConfig() const
{
#ifdef CHARGER_USE_CAN0
    return _pinMapping.can0_rx > 0
        && _pinMapping.can0_tx > 0;
//        && (_pinMapping.can0_stb > 0 || _pinMapping.can0_stb == -1)
//        && (_pinMapping.meanwell_power > 0 || _pinMapping.meanwell_power == -1);
#else
    return _pinMapping.mcp2515_miso > 0
        && _pinMapping.mcp2515_mosi > 0
        && _pinMapping.mcp2515_clk > 0
        && _pinMapping.mcp2515_irq > 0
        && _pinMapping.mcp2515_cs > 0;
//        && (_pinMapping.meanwell_power > 0 || _pinMapping.meanwell_power == -1);
#endif
}
#endif

bool PinMappingClass::isValidPreChargeConfig() const
{
    return _pinMapping.pre_charge >= 0
        && _pinMapping.full_power >= 0;
}
