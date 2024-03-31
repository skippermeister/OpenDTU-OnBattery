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

#ifdef JSON_BUFFER_SIZE
#undef JSON_BUFFER_SIZE
#endif
#define JSON_BUFFER_SIZE 8*1024

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

#ifndef SERIAL1_PIN_RX
#define SERIAL1_PIN_RX 22
#endif

#ifndef SERIAL1_PIN_TX
#define SERIAL1_PIN_TX 21
#endif

#ifndef SERIAL2_PIN_RX
#define SERIAL2_PIN_RX 16
#endif

#ifndef SERIAL2_PIN_TX
#define SERIAL2_PIN_TX 17
#endif

#ifndef SERIAL2_PIN_RTS
#define SERIAL2_PIN_RTS 4
#endif

#ifndef PIN_BMS_WAKEUP
#define PIN_BMS_WAKEUP -1
#endif

#ifdef CHARGER_USE_CAN0
    #ifndef CAN0_PIN_RX
    #define CAN0_PIN_RX 27
    #endif

    #ifndef CAN0_PIN_TX
    #define CAN0_PIN_TX 26
    #endif
    /*
    #ifndef CAN0_PIN_STB
    #define CAN0_PIN_STB    -1
    #endif
    */
#else

    #ifndef MCP2515_PIN_MISO
    #define MCP2515_PIN_MISO 12
    #endif

    #ifndef MCP2515_PIN_MOSI
    #define MCP2515_PIN_MOSI 13
    #endif

    #ifndef MCP2515_PIN_SCLK
    #define MCP2515_PIN_SCLK 26
    #endif

    #ifndef MCP2515_PIN_IRQ
    #define MCP2515_PIN_IRQ 25
    #endif

    #ifndef MCP2515_PIN_CS
    #define MCP2515_PIN_CS 15
    #endif

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
#endif

#if defined(OPENDTU_ETHERNET)
    _pinMapping.eth_enabled = true;
    _pinMapping.eth_phy_addr = ETH_PHY_ADDR;
    _pinMapping.eth_power = ETH_PHY_POWER;
    _pinMapping.eth_mdc = ETH_PHY_MDC;
    _pinMapping.eth_mdio = ETH_PHY_MDIO;
    _pinMapping.eth_type = ETH_PHY_TYPE;
    _pinMapping.eth_clk_mode = ETH_CLK_MODE;
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

    _pinMapping.victron_tx = SERIAL1_PIN_TX;
    _pinMapping.victron_rx = SERIAL1_PIN_RX;

    _pinMapping.victron_tx2 = -1;
    _pinMapping.victron_rx2 = -1;

#if defined(USE_REFUsol_INVERTER)
    _pinMapping.REFUsol_rx = SERIAL1_PIN_RX;
    _pinMapping.REFUsol_tx = SERIAL1_PIN_TX;
    _pinMapping.REFUsol_rts = SERIAL1_PIN_RTS;
#endif

#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
    _pinMapping.battery_rx = SERIAL2_PIN_RX;
    _pinMapping.battery_tx = SERIAL2_PIN_TX;
    _pinMapping.battery_rts = SERIAL2_PIN_RTS;
#if defined(USE_DALYBMS_CONTROLLER)
    _pinMapping.battery_bms_wakeup = PIN_BMS_WAKEUP;
#endif
#else
    _pinMapping.battery_rx = CAN0_PIN_RX;
    _pinMapping.battery_tx = CAN0_PIN_TX;
#endif

    _pinMapping.mcp2515_miso = MCP2515_PIN_MISO;
    _pinMapping.mcp2515_mosi = MCP2515_PIN_MOSI;
    _pinMapping.mcp2515_clk = MCP2515_PIN_SCLK;
    _pinMapping.mcp2515_cs = MCP2515_PIN_CS;
    _pinMapping.mcp2515_irq = MCP2515_PIN_IRQ;

#if defined(CHARGER_HUAWEI)
    _pinMapping.huawei_power = HUAWEI_PIN_POWER;
#else
    _pinMapping.can0_rx = CAN0_PIN_RX;
    _pinMapping.can0_tx = CAN0_PIN_TX;
//    _pinMapping.can0_stb = CAN0_PIN_STB;
#endif

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
#endif

#if  defined(OPENDTU_ETHERNET)
                _pinMapping.eth_enabled = doc[i]["eth"]["enabled"] | true;
                _pinMapping.eth_phy_addr = doc[i]["eth"]["phy_addr"] | ETH_PHY_ADDR;
                _pinMapping.eth_power = doc[i]["eth"]["power"] | ETH_PHY_POWER;
                _pinMapping.eth_mdc = doc[i]["eth"]["mdc"] | ETH_PHY_MDC;
                _pinMapping.eth_mdio = doc[i]["eth"]["mdio"] | ETH_PHY_MDIO;
                _pinMapping.eth_type = doc[i]["eth"]["type"] | ETH_PHY_TYPE;
                _pinMapping.eth_clk_mode = doc[i]["eth"]["clk_mode"] | ETH_CLK_MODE;
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
                _pinMapping.victron_rx = doc[i]["victron"]["rs323_rx"] | SERIAL1_PIN_RX;
                _pinMapping.victron_tx = doc[i]["victron"]["rs323_tx"] | SERIAL1_PIN_TX;

                _pinMapping.victron_rx2 = doc[i]["victron"]["rs323_rx2"] | -1;
                _pinMapping.victron_tx2 = doc[i]["victron"]["rs323_tx2"] | -1;

#if defined(USE_REFUsol_INVERTER)
                _pinMapping.REFUsol_rx = doc[i]["refusol"]["rs485_rx"] | SERIAL1_PIN_RX;
                _pinMapping.REFUsol_tx = doc[i]["refusol"]["rs485_tx"] | SERIAL1_PIN_TX;
                if (doc[i]["refusol"].containsKey("rs485_rts")) {
                    _pinMapping.REFUsol_rts = doc[i]["refusol"]["rs485_rts"] | SERIAL1_PIN_RTS;
                } else {
                    _pinMapping.REFUsol_rts = -1;
                }
#endif

#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKMS_CONTROLLER)
                if (doc[i]["battery"].containsKey("rs485_rx")) {
                    _pinMapping.battery_rx = doc[i]["battery"]["rs485_rx"] | SERIAL2_PIN_RX;
                    _pinMapping.battery_tx = doc[i]["battery"]["rs485_tx"] | SERIAL2_PIN_TX;
                    if (doc[i]["battery"].containsKey("rs485_rts")) {
                        _pinMapping.battery_rts = doc[i]["battery"]["rs485_rts"] | SERIAL2_PIN_RTS;
                    } else {
                        _pinMapping.battery_rts = -1;
                    }
                } else
#endif
#if defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
                 if (doc[i]["battery"].containsKey("rs232_rx")) {
                    _pinMapping.battery_rx = doc[i]["battery"]["rs232_rx"] | SERIAL2_PIN_RX;
                    _pinMapping.battery_tx = doc[i]["battery"]["rs232_tx"] | SERIAL2_PIN_TX;
                    _pinMapping.battery_rts = -2; // indicate we have a RS232 interface
                } else
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER)
                 if (doc[i]["battery"].containsKey("can0_rx")) {
                    _pinMapping.battery_rx = doc[i]["battery"]["can0_rx"] | CAN0_PIN_RX;
                    _pinMapping.battery_tx = doc[i]["battery"]["can0_tx"] | CAN0_PIN_TX;
                } else
#endif
                {
                    _pinMapping.battery_rx = SERIAL2_PIN_RX;
                    _pinMapping.battery_tx = SERIAL2_PIN_TX;
                    _pinMapping.battery_rts = SERIAL2_PIN_RTS;
                }
#if defined(USE_DALYBMS_CONTROLLER)
                _pinMapping.battery_bms_wakeup = doc[i]["battery"]["bms_wakeup"] | PIN_BMS_WAKEUP;
#endif

#if defined(CHARGER_HUAWEI)
                _pinMapping.huawei_power = doc[i]["huawei"]["power"] | HUAWEI_PIN_POWER;
                _pinMapping.mcp2515_miso = doc[i]["huawei"]["mcp2515_miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["huawei"]["mcp2515_mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["huawei"]["mcp2515_clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["huawei"]["mcp2515_irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["huawei"]["mcp2515_cs"] | MCP2515_PIN_CS;
#else
#if defined(CHARGER_USE_CAN0)
                _pinMapping.can0_rx = doc[i]["charger"]["can0_rx"] | CAN0_PIN_RX;
                _pinMapping.can0_tx = doc[i]["charger"]["can0_tx"] | CAN0_PIN_TX;
                // _pinMapping.can0_stb = doc[i]["charger"]["can0_stb"] | CAN0_PIN_STB;

                _pinMapping.mcp2515_miso = doc[i]["mcp2515"]["miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["mcp2515"]["mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["mcp2515"]["clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["mcp2515"]["irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["mcp2515"]["cs"] | MCP2515_PIN_CS;
#else
                _pinMapping.mcp2515_miso = doc[i]["charger"]["mcp2515_miso"] | MCP2515_PIN_MISO;
                _pinMapping.mcp2515_mosi = doc[i]["charger"]["mcp2515_mosi"] | MCP2515_PIN_MOSI;
                _pinMapping.mcp2515_clk = doc[i]["charger"]["mcp2515_clk"] | MCP2515_PIN_SCLK;
                _pinMapping.mcp2515_irq = doc[i]["charger"]["mcp2515_irq"] | MCP2515_PIN_IRQ;
                _pinMapping.mcp2515_cs = doc[i]["charger"]["mcp2515_cs"] | MCP2515_PIN_CS;
#endif
#endif
                _pinMapping.pre_charge = doc[i]["batteryConnectedInverter"]["pre_charge"] | PRE_CHARGE_PIN;
                _pinMapping.full_power = doc[i]["batteryConnectedInverter"]["full_power"] | FULL_POWER_PIN;

                if (doc[i]["powermeter"].containsKey("sml_rs232_rx")) { // SML Interface
                    _pinMapping.powermeter_rx = doc[i]["powermeter"]["sml_rs232_rx"] | POWERMETER_PIN_RX;
                    _pinMapping.powermeter_tx = doc[i]["powermeter"]["sml_rs232_tx"] | POWERMETER_PIN_TX;
                    _pinMapping.powermeter_rts = -1;
                } else if (doc[i]["powermeter"].containsKey("sdm_rs485_rx")) {  // SDM Interface
                    _pinMapping.powermeter_rx = doc[i]["powermeter"]["sdm_rs485_rx"] | POWERMETER_PIN_RX;
                    _pinMapping.powermeter_tx = doc[i]["powermeter"]["sdm_rs485_tx"] | POWERMETER_PIN_TX;
                    if (doc[i]["powermeter"].containsKey("sdm_rs485_rts")) {
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
        && _pinMapping.cmt_sdio > 0;
}
#endif

#if defined(OPENDTU_ETHERNET)
bool PinMappingClass::isValidEthConfig() const
{
    return _pinMapping.eth_enabled;
}
#endif

bool PinMappingClass::isValidBatteryConfig() const
{
    return    _pinMapping.battery_rx > 0
           && _pinMapping.battery_tx >= 0
           && (_pinMapping.battery_rx != _pinMapping.battery_tx)
           && (_pinMapping.battery_rts != _pinMapping.battery_rx)
           && (_pinMapping.battery_rts != _pinMapping.battery_tx)
#if defined (USE_DALYBMS_CONTROLLER)
           && (_pinMapping.battery_bms_wakeup != _pinMapping.battery_rx)
           && (_pinMapping.battery_bms_wakeup != _pinMapping.battery_tx)
#endif
           ;
}

#if defined(CHARGER_HUAWEI)
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
#if defined(CHARGER_USE_CAN0)
    return _pinMapping.can0_rx > 0
        && _pinMapping.can0_tx > 0;
//        && (_pinMapping.can0_stb > 0 || _pinMapping.can0_stb == -1);
#else
    return _pinMapping.mcp2515_miso > 0
        && _pinMapping.mcp2515_mosi > 0
        && _pinMapping.mcp2515_clk > 0
        && _pinMapping.mcp2515_irq > 0
        && _pinMapping.mcp2515_cs > 0;
#endif
}
#endif

bool PinMappingClass::isValidPreChargeConfig() const
{
    return _pinMapping.pre_charge >= 0
        && _pinMapping.full_power >= 0;
}

void PinMappingClass::createPinMappingJson() const
{
    DynamicJsonDocument obj(JSON_BUFFER_SIZE);

    JsonArray array = obj.createNestedArray("");
    JsonObject doc = array.createNestedObject();

    doc["name"] = "my_very_special_board";

#if defined(USE_RADIO_NRF)
    JsonObject nrf24 = doc.createNestedObject("nrf24");
    nrf24["clk"]  = _pinMapping.nrf24_clk;
    nrf24["cs"]   = _pinMapping.nrf24_cs;
    nrf24["en"]   = _pinMapping.nrf24_en;
    nrf24["irq"]  = _pinMapping.nrf24_irq;
    nrf24["miso"] = _pinMapping.nrf24_miso;
    nrf24["mosi"] = _pinMapping.nrf24_mosi;
#endif

#if defined(USE_RADIO_CMT)
    JsonObject cmt = doc.createNestedObject("cmt");
    cmt["clk"]   = _pinMapping.cmt_clk;
    cmt["cs"]    = _pinMapping.cmt_cs;
    cmt["fcs"]   = _pinMapping.cmt_fcs;
    cmt["gpio2"] = _pinMapping.cmt_gpio2;
    cmt["gpio3"] = _pinMapping.cmt_gpio3;
    cmt["sdio"]  = _pinMapping.cmt_sdio;
#endif

#if defined(OPENDTU_ETHERNET)
    JsonObject eth = doc.createNestedObject("eth");
    eth["enabled"]  = _pinMapping.eth_enabled;
    eth["phy_addr"] = _pinMapping.eth_phy_addr;
    eth["power"]    = _pinMapping.eth_power;
    eth["mdc"]      = _pinMapping.eth_mdc;
    eth["mdio"]     = _pinMapping.eth_mdio;
    eth["type"]     = _pinMapping.eth_type;
    eth["clk_mode"] = _pinMapping.eth_clk_mode;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    JsonObject display = doc.createNestedObject("display");
    display["type"]  = _pinMapping.display_type;
    display["data"]  = _pinMapping.display_data;
    display["clk"]   = _pinMapping.display_clk;
    display["cs"]    = _pinMapping.display_cs;
    display["reset"] = _pinMapping.display_reset;
    display["busy"]  = _pinMapping.display_busy;
    display["dc"]    = _pinMapping.display_dc;
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    JsonObject led = doc.createNestedObject("led");
    led["led0"] = _pinMapping.led[0];
    led["led1"] = _pinMapping.led[1];
#if defined(USE_LED_STRIP)
    led["rgb"] = _pinMapping.led_rgb;
#endif
#endif

    JsonObject victron = doc.createNestedObject("victron");
    victron["rs232_rx"] = _pinMapping.victron_rx;
    victron["rs232_tx"] = _pinMapping.victron_tx;
    victron["rs232_rx2"] = _pinMapping.victron_rx2;
    victron["rs232_tx2"] = _pinMapping.victron_tx2;

#if defined(USE_REFUsol_INVERTER)
    JsonObject refusol = doc.createNestedObject("refusol");
    refusol["rs485_rx"] = _pinMapping.REFUsol_rx;
    refusol["rs485_tx"] = _pinMapping.REFUsol_tx;
    if (_pinMapping.REFUsol_rts >= 0) refusol["rs485_rts"] = _pinMapping.REFUsol_rts;
#endif

    JsonObject battery = doc.createNestedObject("battery");
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
    if (_pinMapping.battery_rts >= -1) {
        battery["rs485_rx"]  = _pinMapping.battery_rx;
        battery["rs485_tx"]  = _pinMapping.battery_tx;
        if (_pinMapping.battery_rts >= 0) battery["rs485_rts"] = _pinMapping.battery_rts;
    } else {
        battery["rs232_rx"]  = _pinMapping.battery_rx;
        battery["rs232_tx"]  = _pinMapping.battery_tx;
    }
#if defined(USE_DALYBMS_CONTROLLER)
    battery["bms_wakeup"] = _pinMapping.battery_bms_wakeup;
#endif
#else
    battery["can0_rx"] = _pinMapping.battery_rx;
    battery["can0_tx"] = _pinMapping.battery_tx;
#endif

    //JsonObject huawei = doc.createNestedObject("huawei");
    JsonObject charger = doc.createNestedObject("charger");
    JsonObject mcp2515 = doc.createNestedObject("mcp2515");
#if defined(CHARGER_HUAWEI)
    huawei["power"] = _pinMapping.huawei_power;
    huawei["mcp2515_miso"] = _pinMapping.mcp2515_miso;
    huawei["mcp2515_mosi"] = _pinMapping.mcp2515_mosi;
    huawei["mcp2515_clk"]  = _pinMapping.mcp2515_clk;
    huawei["mcp2515_irq"]  = _pinMapping.mcp2515_irq;
    huawei["mcp2515_cs"]   = _pinMapping.mcp2515_cs;
#else
#if defined(CHARGER_USE_CAN0)
    charger["can0_rx"] = _pinMapping.can0_rx;
    charger["can0_tx"] = _pinMapping.can0_tx;
    // charger["can0_stb"] = _pinMapping.can0_stb;

    mcp2515["miso"] = _pinMapping.mcp2515_miso;
    mcp2515["mosi"] = _pinMapping.mcp2515_mosi;
    mcp2515["clk"]  = _pinMapping.mcp2515_clk;
    mcp2515["irq"]  = _pinMapping.mcp2515_irq;
    mcp2515["cs"]   = _pinMapping.mcp2515_cs;
#else
    charger["mcp2515_miso"] = _pinMapping.mcp2515_miso;
    charger["mcp2515_mosi"] = _pinMapping.mcp2515_mosi;
    charger["mcp2515_clk"] = _pinMapping.mcp2515_clk;
    charger["mcp2515_irq"] = _pinMapping.mcp2515_irq;
    charger["mcp2515_cs"] = _pinMapping.mcp2515_cs;
#endif
#endif
    JsonObject battery_connected_inverter = doc.createNestedObject("batteryConnectedInverter");
    battery_connected_inverter["pre_charge"] = _pinMapping.pre_charge;
    battery_connected_inverter["full_power"] = _pinMapping.full_power;

    PowerMeterClass::Source source = static_cast<PowerMeterClass::Source>(Configuration.get().PowerMeter.Source);
    JsonObject powermeter = doc.createNestedObject("powermeter");
    if (source == PowerMeterClass::Source::SML)
    {
        powermeter["sml_rs232_rx"] = _pinMapping.powermeter_rx;
        if (_pinMapping.powermeter_tx >= 0) powermeter["sml_rs232_tx"] = _pinMapping.powermeter_tx;
    } else if (   source == PowerMeterClass::Source::SDM1PH
               || source == PowerMeterClass::Source::SDM3PH)
    {
        powermeter["sdm_rs485_rx"] = _pinMapping.powermeter_rx;
        powermeter["sdm_rs485_tx"] = _pinMapping.powermeter_tx;
        if (_pinMapping.powermeter_rts >= 0) powermeter["sdm_rs485_rts"] = _pinMapping.powermeter_rts;
    }

    String buffer;
    serializeJsonPretty(array, buffer);

    Serial.println(buffer);
}
