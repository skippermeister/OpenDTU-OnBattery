// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_device.h"
#include "Configuration.h"
#include "Display_Graphic.h"
#include "PowerMeter.h"
#include "PinMapping.h"
#include "Utils.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include <AsyncJson.h>

void WebApiDeviceClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/device/config", HTTP_GET, std::bind(&WebApiDeviceClass::onDeviceAdminGet, this, _1));
    server.on("/api/device/config", HTTP_POST, std::bind(&WebApiDeviceClass::onDeviceAdminPost, this, _1));
}

void WebApiDeviceClass::onDeviceAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const CONFIG_T& config = Configuration.get();
    const PinMapping_t& pin = PinMapping.get();

    auto curPin = root["curPin"].to<JsonObject>();
    curPin["name"] = config.Dev_PinMapping;

    auto prechargePinObj = curPin["batteryConnectedInverter"].to<JsonObject>();
    prechargePinObj["pre_charge"] = pin.pre_charge;
    prechargePinObj["full_power"] = pin.full_power;

#if defined(USE_RADIO_NRF)
    auto nrfPinObj = curPin["nrf24"].to<JsonObject>();
    if (PinMapping.isValidNrf24Config()) {
        nrfPinObj["clk"] = pin.nrf24_clk;
        nrfPinObj["cs"] = pin.nrf24_cs;
        nrfPinObj["en"] = pin.nrf24_en;
        nrfPinObj["irq"] = pin.nrf24_irq;
        nrfPinObj["miso"] = pin.nrf24_miso;
        nrfPinObj["mosi"] = pin.nrf24_mosi;
    } else {
        nrfPinObj["Pins"] = "invalid";
    }
#endif

#if defined(USE_RADIO_CMT)
    auto cmtPinObj = curPin["cmt"].to<JsonObject>();
    if (PinMapping.isValidCmt2300Config()) {
        cmtPinObj["clk"] = pin.cmt_clk;
        cmtPinObj["cs"] = pin.cmt_cs;
        cmtPinObj["fcs"] = pin.cmt_fcs;
        cmtPinObj["sdio"] = pin.cmt_sdio;
        cmtPinObj["gpio2"] = pin.cmt_gpio2;
        cmtPinObj["gpio3"] = pin.cmt_gpio3;
        cmtPinObj["chip_int1gpio"] = pin.cmt_chip_int1gpio;
        cmtPinObj["chip_int2gpio"] = pin.cmt_chip_int2gpio;
    } else {
        cmtPinObj["Pins"] = "invalid";
    }
#endif

#if defined(OPENDTU_ETHERNET)
    auto ethPinObj = curPin["eth"].to<JsonObject>();
    ethPinObj["enabled"] = pin.eth_enabled;
    ethPinObj["phy_addr"] = pin.eth_phy_addr;
    ethPinObj["power"] = pin.eth_power;
    ethPinObj["mdc"] = pin.eth_mdc;
    ethPinObj["mdio"] = pin.eth_mdio;
    ethPinObj["type"] = pin.eth_type;
    ethPinObj["clk_mode"] = pin.eth_clk_mode;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    auto displayPinObj = curPin["display"].to<JsonObject>();
    if (pin.display_data < 0 && pin.display_clk < 0 && pin.display_cs < 0 && pin.display_reset < 0 && pin.display_busy < 0 && pin.display_dc < 0) {
        ;
    } else {
        displayPinObj["type"] = pin.display_type;
        if (pin.display_type == DisplayType_t::PCD8544_HW_SPI || pin.display_type == DisplayType_t::PCD8544_SW_SPI || pin.display_type == DisplayType_t::ePaper154) {
            displayPinObj["cs"] = pin.display_cs;
        }
        if (pin.display_type != DisplayType_t::None) {
            displayPinObj["data"] = pin.display_data;
            displayPinObj["clk"] = pin.display_clk;
            displayPinObj["reset"] = pin.display_reset;
        }
        if (pin.display_type == DisplayType_t::ePaper154) {
            displayPinObj["busy"] = pin.display_busy;
        }
        if (pin.display_type == DisplayType_t::ePaper154 || pin.display_type == DisplayType_t::PCD8544_SW_SPI) {
            displayPinObj["dc"] = pin.display_dc;
        }
    }
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    auto ledPinObj = curPin["led"].to<JsonObject>();
#if defined(USE_LED_SINGLE)
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        ledPinObj["led" + String(i)] = pin.led[i];
    }
#endif
#if defined(USE_LED_STRIP)
    ledPinObj["rgb"] = pin.led_rgb;
#endif
    auto leds = root["led"].to<JsonArray>();
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        JsonObject led = leds.add<JsonObject>();
        led["brightness"] = config.Led[i].Brightness;
    }
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    auto display = root["display"].to<JsonObject>();
    display["rotation"] = config.Display.Rotation;
    display["power_safe"] = config.Display.PowerSafe;
    display["screensaver"] = config.Display.ScreenSaver;
    display["contrast"] = config.Display.Contrast;
    display["language"] = config.Display.Language;
    display["diagramduration"] = config.Display.Diagram.Duration;
    display["diagrammode"] = config.Display.Diagram.Mode;
    display["typedescription"] = pin.display_type == DisplayType_t::None                ? "None"
                               : pin.display_type == DisplayType_t::PCD8544_HW_SPI      ? "PCD8544 (HW SPI)"
                               : pin.display_type == DisplayType_t::PCD8544_SW_SPI      ? "PCD8544 (SW SPI)"
                               : pin.display_type == DisplayType_t::SSD1306             ? "SSD1306 (I2C)"
                               : pin.display_type == DisplayType_t::SH1106              ? "SH1106 (I2C)"
                               : pin.display_type == DisplayType_t::SSD1309             ? "SSD1309 (I2C)"
                               : pin.display_type == DisplayType_t::ST7567_GM12864I_59N ? "ST7567 (I2C)"
                               : pin.display_type == DisplayType_t::ePaper154           ? "ePaper154 (SW SPI)"
                                                                                        : "unknown";
#endif

    auto victronPinObj = curPin["victron"].to<JsonObject>();
    for (int i=0; i<sizeof(pin.victron)/sizeof(RS232_t); i++) {
        if (pin.victron[i].rx >= 0) {
            String offset = (i>0)?String(i+1):String("");
            victronPinObj[String("rs232_rx")+offset] = pin.victron[i].rx;
            victronPinObj[String("rs232_tx")+offset] = pin.victron[i].tx;
        }
    }

#if defined(USE_REFUsol_INVERTER)
    auto refusolPinObj = curPin["refusol"].to<JsonObject>();
    refusolPinObj["rs485_rx"] = pin.REFUsol.rx;
    refusolPinObj["rs485_tx"] = pin.REFUsol.tx;
    if (pin.REFUsol.rts >= 0) {
        refusolPinObj["rs485_rts"] = pin.REFUsol.rts;
    }
#endif

    auto batteryPinObj = curPin["battery"].to<JsonObject>();
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
    if (pin.battery.provider == Battery_Provider_t::RS485) {
        batteryPinObj["rs485_rx"] = pin.battery.rs485.rx;
        batteryPinObj["rs485_tx"] = pin.battery.rs485.tx;
        if (pin.battery.rs485.rts >= 0) {
            batteryPinObj["rs485_rts"] = pin.battery.rs485.rts;
        }
#if defined(USE_DALYBMS_CONTROLLER)
        if (pin.battery.wakeup >= 0) batteryPinObj["wakeup"] = pin.battery.wakeup;
#endif
    } else if (pin.battery.provider == Battery_Provider_t::RS232) {
        batteryPinObj["rs232_rx"] = pin.battery.rs232.rx;
        batteryPinObj["rs232_tx"] = pin.battery.rs232.tx;
#if defined(USE_DALYBMS_CONTROLLER)
        if (pin.battery.wakeup >= 0) batteryPinObj["wakeup"] = pin.battery.wakeup;
#endif
    }
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)
    else
#endif
#endif
#if defined(USE_PYLONTECH_CAN_RECEIVER) || defined(USE_PYTES_CAN_RECEIVER)
    if (pin.battery.provider == Battery_Provider_t::CAN0) {
        batteryPinObj["can0_rx"] = pin.battery.can0.rx;
        batteryPinObj["can0_tx"] = pin.battery.can0.tx;
    } else if (pin.battery.provider == Battery_Provider_t::I2C0) {
        batteryPinObj["i2c0_scl"] = pin.battery.i2c.scl;
        batteryPinObj["i2c0_sda"] = pin.battery.i2c.sda;
    } else if (pin.battery.provider == Battery_Provider_t::I2C1) {
        batteryPinObj["i2c1_scl"] = pin.battery.i2c.scl;
        batteryPinObj["i2c1_sda"] = pin.battery.i2c.sda;
    } else if (pin.battery.provider == Battery_Provider_t::MCP2515) {
        batteryPinObj["mcp2515_miso"] = pin.battery.mcp2515.miso;
        batteryPinObj["mcp2515_mosi"] = pin.battery.mcp2515.mosi;
        batteryPinObj["mcp2515_clk"] = pin.battery.mcp2515.clk;
        batteryPinObj["mcp2515_irq"] = pin.battery.mcp2515.irq;
        batteryPinObj["mcp2515_cs"] = pin.battery.mcp2515.cs;
    }
#endif

    auto chargerPinObj = curPin["charger"].to<JsonObject>();
#if defined(USE_CHARGER_HUAWEI)
    chargerPinObj["power"] = pin.charger.power;
#endif

#if defined(USE_CHARGER_MEANWELL) || defined(USE_CHARGER_HUAWEI)
    if (pin.charger.provider == Charger_Provider_t::CAN0) {
        chargerPinObj["can0_rx"] = pin.charger.can0.rx;
        chargerPinObj["can0_tx"] = pin.charger.can0.tx;
    //    chargerPinObj["can0_stb"] = pin.charger.can0.stb;
    }
    if (pin.charger.provider == Charger_Provider_t::I2C0) {
        chargerPinObj["i2c0_scl"] = pin.charger.i2c.scl;
        chargerPinObj["i2c0_sda"] = pin.charger.i2c.sda;
    }
    if (pin.charger.provider == Charger_Provider_t::I2C1) {
        chargerPinObj["i2c1_scl"] = pin.charger.i2c.scl;
        chargerPinObj["i2c1_sda"] = pin.charger.i2c.sda;
    }
    if (pin.charger.provider == Charger_Provider_t::MCP2515) {
        chargerPinObj["mcp2515_miso"] = pin.charger.mcp2515.miso;
        chargerPinObj["mcp2515_mosi"] = pin.charger.mcp2515.mosi;
        chargerPinObj["mcp2515_clk"] = pin.charger.mcp2515.clk;
        chargerPinObj["mcp2515_irq"] = pin.charger.mcp2515.irq;
        chargerPinObj["mcp2515_cs"] = pin.charger.mcp2515.cs;
    }
#endif

    if (static_cast<PowerMeterProvider::Type>(config.PowerMeter.Source) == PowerMeterProvider::Type::SERIAL_SML)
    {
        auto smlPinObj = curPin["powermeter"].to<JsonObject>();
        smlPinObj["sml_rs232_rx"] = pin.powermeter_rx;
        if (pin.powermeter_tx >= 0) smlPinObj["sml_rs232_tx"] = pin.powermeter_tx;
    } else if (   static_cast<PowerMeterProvider::Type>(config.PowerMeter.Source) == PowerMeterProvider::Type::SDM1PH
               || static_cast<PowerMeterProvider::Type>(config.PowerMeter.Source) == PowerMeterProvider::Type::SDM3PH)
    {
        auto sdmPinObj = curPin["powermeter"].to<JsonObject>();
        sdmPinObj["sdm_rs485_rx"] = pin.powermeter_rx;
        sdmPinObj["sdm_rs485_tx"] = pin.powermeter_tx;
        if (pin.powermeter_rts >= 0) {
            sdmPinObj["sdm_rs485_rts"] = pin.powermeter_rts;
        }
    }
/*
        String output;
        serializeJsonPretty(root, output);
        Serial.println(output);
*/
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiDeviceClass::onDeviceAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root.containsKey("curPin")
#if defined(USE_DISPLAY_GRAPHIC)
    || root.containsKey("display")
#endif
        )) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["curPin"]["name"].as<String>().length() == 0 || root["curPin"]["name"].as<String>().length() > DEV_MAX_MAPPING_NAME_STRLEN) {
        retMsg["message"] = "Pin mapping must between 1 and " STR(DEV_MAX_MAPPING_NAME_STRLEN) " characters long!";
        retMsg["code"] = WebApiError::HardwarePinMappingLength;
        retMsg["param"]["max"] = DEV_MAX_MAPPING_NAME_STRLEN;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

/*    String output;
    serializeJson(root, output);
    Serial.println(output);
*/
    CONFIG_T& config = Configuration.get();
    bool performRestart = root["curPin"]["name"].as<String>() != config.Dev_PinMapping;

    strlcpy(config.Dev_PinMapping, root["curPin"]["name"].as<String>().c_str(), sizeof(config.Dev_PinMapping));
#if defined(USE_DISPLAY_GRAPHIC)
    config.Display.Rotation = root["display"]["rotation"].as<uint8_t>();
    config.Display.PowerSafe = root["display"]["power_safe"].as<bool>();
    config.Display.ScreenSaver = root["display"]["screensaver"].as<bool>();
    config.Display.Contrast = root["display"]["contrast"].as<uint8_t>();
    config.Display.Language = root["display"]["language"].as<uint8_t>();
    config.Display.Diagram.Duration = root["display"]["diagramduration"].as<uint32_t>();
    config.Display.Diagram.Mode = root["display"]["diagrammode"].as<DiagramMode_t>();
#endif

#if defined(USE_LED_SINGLE) || defined(USE_LED_STRIP)
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        config.Led[i].Brightness = root["led"][i]["brightness"].as<uint8_t>();
        config.Led[i].Brightness = min<uint8_t>(100, config.Led[i].Brightness);
    }
#endif

#ifdef USE_DISPLAY_GRAPHIC
    Display.setDiagramMode(static_cast<DiagramMode_t>(config.Display.Diagram.Mode));
    Display.setOrientation(config.Display.Rotation);
    Display.enablePowerSafe = config.Display.PowerSafe;
    Display.enableScreensaver = config.Display.ScreenSaver;
    Display.setContrast(config.Display.Contrast);
    Display.setLanguage(config.Display.Language);
    Display.Diagram().updatePeriod();
#endif

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    if (performRestart) {
        Utils::restartDtu();
    }
}
