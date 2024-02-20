// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_device.h"
#include "Configuration.h"
#include "Display_Graphic.h"
#include "PinMapping.h"
#include "Utils.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include <AsyncJson.h>

void WebApiDeviceClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/device/config", HTTP_GET, std::bind(&WebApiDeviceClass::onDeviceAdminGet, this, _1));
    _server->on("/api/device/config", HTTP_POST, std::bind(&WebApiDeviceClass::onDeviceAdminPost, this, _1));
}

void WebApiDeviceClass::onDeviceAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, MQTT_JSON_DOC_SIZE);
    auto& root = response->getRoot();
    const CONFIG_T& config = Configuration.get();
    const PinMapping_t& pin = PinMapping.get();

    auto curPin = root.createNestedObject("curPin");
    curPin["name"] = config.Dev_PinMapping;

    auto prechargePinObj = curPin.createNestedObject("batteryConnectedInverter");
    prechargePinObj["pre_charge"] = pin.pre_charge;
    prechargePinObj["full_power"] = pin.full_power;

    auto nrfPinObj = curPin.createNestedObject("nrf24");
    if (PinMapping.isValidNrf24Config()) {
        nrfPinObj["clk"] = pin.nrf24_clk;
        nrfPinObj["cs"] = pin.nrf24_cs;
        nrfPinObj["en"] = pin.nrf24_en;
        nrfPinObj["irq"] = pin.nrf24_irq;
        nrfPinObj["miso"] = pin.nrf24_miso;
        nrfPinObj["mosi"] = pin.nrf24_mosi;
    } else {
        nrfPinObj["enabled"] = false;
    }

    auto cmtPinObj = curPin.createNestedObject("cmt");
#ifdef USE_RADIO_CMT
    if (PinMapping.isValidCmt2300Config()) {
        cmtPinObj["clk"] = pin.cmt_clk;
        cmtPinObj["cs"] = pin.cmt_cs;
        cmtPinObj["fcs"] = pin.cmt_fcs;
        cmtPinObj["sdio"] = pin.cmt_sdio;
        cmtPinObj["gpio2"] = pin.cmt_gpio2;
        cmtPinObj["gpio3"] = pin.cmt_gpio3;
    } else
#endif
    {
        cmtPinObj["enabled"] = false;
    }

    auto ethPinObj = curPin.createNestedObject("eth");
    ethPinObj["enabled"] = false;
#ifdef OPENDTU_ETHERNET
    ethPinObj["enabled"] = pin.eth_enabled;
    ethPinObj["phy_addr"] = pin.eth_phy_addr;
    ethPinObj["power"] = pin.eth_power;
    ethPinObj["mdc"] = pin.eth_mdc;
    ethPinObj["mdio"] = pin.eth_mdio;
    ethPinObj["type"] = pin.eth_type;
    ethPinObj["clk_mode"] = pin.eth_clk_mode;
#endif

    auto displayPinObj = curPin.createNestedObject("display");
#ifdef USE_DISPLAY_GRAPHIC
    if (pin.display_data < 0 && pin.display_clk < 0 && pin.display_cs < 0 && pin.display_reset < 0 && pin.display_busy < 0 && pin.display_dc < 0) {
        displayPinObj["enabled"] = false;
    } else {
        displayPinObj["type"] = pin.display_type == DisplayType_t::None ? "None" : pin.display_type == DisplayType_t::PCD8544_HW_SPI ? "PCD8544 (HW SPI)"
            : pin.display_type == DisplayType_t::PCD8544_SW_SPI                                                                      ? "PCD8544 (SW SPI)"
            : pin.display_type == DisplayType_t::SSD1306                                                                             ? "SSD1306 (I2C)"
            : pin.display_type == DisplayType_t::SH1106                                                                              ? "SH1106 (I2C)"
            : pin.display_type == DisplayType_t::SSD1309                                                                             ? "SSD1309 (I2C)"
            : pin.display_type == DisplayType_t::ST7567_GM12864I_59N                                                                 ? "ST7567 (I2C)"
            : pin.display_type == DisplayType_t::ePaper154                                                                           ? "ePaper154 (SW SPI)"
                                                                                                                                     : "unknown";
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
#else
    displayPinObj["enabled"] = false;
#endif

    auto ledPinObj = curPin.createNestedObject("led");
    bool ledActive = false;
#ifdef USE_LED_SINGLE
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        if (pin.led[i] >= 0) {
            ledPinObj["led" + String(i)] = pin.led[i];
            ledActive = true;
        }
    }
#endif

#ifdef USER_LED_STRIP
    if (pin.led_rgb >= 0) {
        ledPinObj["rgb"] = pin.led_rgb;
        ledActive = true;
    }
#endif
    if (ledActive == false)
        ledPinObj["enabled"] = false;

    auto leds = root.createNestedArray("led");
#ifdef USE_LED_SINGLE
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        JsonObject led = leds.createNestedObject();
        led["brightness"] = config.Led_Single[i].Brightness;
    }
#endif

    auto display = root.createNestedObject("display");
#ifdef USE_DISPLAY_GRAPHIC
    display["rotation"] = config.Display.Rotation;
    display["power_safe"] = config.Display.PowerSafe;
    display["screensaver"] = config.Display.ScreenSaver;
    display["contrast"] = config.Display.Contrast;
    display["language"] = config.Display.Language;
    display["diagramduration"] = config.Display.Diagram.Duration;
    display["diagrammode"] = config.Display.Diagram.Mode;
#else
    display["rotation"] = 2;
    display["power_safe"] = false;
    display["screensaver"] = false;
    display["contrast"] = 60;
    display["language"] = 0;
    display["diagramduration"] = 36000;
    display["diagrammode"] = 1;
#endif

    auto victronPinObj = curPin.createNestedObject("victron");
    victronPinObj["rs232_rx"] = pin.victron_rx;
    victronPinObj["rs232_tx"] = pin.victron_tx;

#ifdef USE_REFUsol_INVERTER
    auto refusolPinObj = curPin.createNestedObject("refusol");
    refusolPinObj["rs485_rx"] = pin.REFUsol_rx;
    refusolPinObj["rs485_tx"] = pin.REFUsol_tx;
    refusolPinObj["rs485_rts"] = pin.REFUsol_rts;
#endif

    auto batteryPinObj = curPin.createNestedObject("battery");
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER)
    batteryPinObj["rs485_rx"] = pin.battery_rx;
    batteryPinObj["rs485_tx"] = pin.battery_tx;
    batteryPinObj["rs485_rts"] = pin.battery_rts;
#else
    batteryPinObj["can0_rx"] = pin.battery_rx;
    batteryPinObj["can0_tx"] = pin.battery_tx;
#endif

    auto chargerPinObj = curPin.createNestedObject("charger");
#ifdef CHARGER_HUAWEI
    chargerPinObj["power"] = pin.huawei_power;
#else
//     chargerPinObj["power"] = pin.meanwell_power;
#endif
#ifdef CHARGER_USE_CAN0
    chargerPinObj["can0_rx"] = pin.can0_rx;
    chargerPinObj["can0_tx"] = pin.can0_tx;
    //    chargerPinObj["can0_stb"] = pin.can0_stb;

    auto mcp2515PinObj = curPin.createNestedObject("mcp2515");
    mcp2515PinObj["miso"] = pin.mcp2515_miso;
    mcp2515PinObj["mosi"] = pin.mcp2515_mosi;
    mcp2515PinObj["clk"] = pin.mcp2515_clk;
    mcp2515PinObj["irq"] = pin.mcp2515_irq;
    mcp2515PinObj["cs"] = pin.mcp2515_cs;
#else
    chargerPinObj["mcp2515_miso"] = pin.mcp2515_miso;
    chargerPinObj["mcp2515_mosi"] = pin.mcp2515_mosi;
    chargerPinObj["mcp2515_clk"] = pin.mcp2515_clk;
    chargerPinObj["mcp2515_irq"] = pin.mcp2515_irq;
    chargerPinObj["mcp2515_cs"] = pin.mcp2515_cs;
#endif

    auto sdmPinObj = curPin.createNestedObject("sdm");
    sdmPinObj["rs485_rx"] = pin.sdm_rx;
    sdmPinObj["rs485_tx"] = pin.sdm_tx;
    sdmPinObj["rs485_rts"] = pin.sdm_rts;

    auto smlPinObj = curPin.createNestedObject("sml");
    smlPinObj["rs232_rx"] = pin.sml_rx;

    /*
        String output;
        serializeJsonPretty(root, output);
        MessageOutput.println(output);
    */
    response->setLength();
    request->send(response);
}

void WebApiDeviceClass::onDeviceAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, MQTT_JSON_DOC_SIZE);
    auto& retMsg = response->getRoot();
    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = NoValuesFound;
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > MQTT_JSON_DOC_SIZE) {
        retMsg["message"] = DataTooLarge;
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(MQTT_JSON_DOC_SIZE);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = FailedToParseData;
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("curPin") || root.containsKey("display"))) {
        retMsg["message"] = ValuesAreMissing;
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    if (root["curPin"]["name"].as<String>().length() == 0 || root["curPin"]["name"].as<String>().length() > DEV_MAX_MAPPING_NAME_STRLEN) {
        retMsg["message"] = "Pin mapping must between 1 and " STR(DEV_MAX_MAPPING_NAME_STRLEN) " characters long!";
        retMsg["code"] = WebApiError::HardwarePinMappingLength;
        retMsg["param"]["max"] = DEV_MAX_MAPPING_NAME_STRLEN;
        response->setLength();
        request->send(response);
        return;
    }

    String output;
    serializeJson(root, output);
    Serial.println(output);

    CONFIG_T& config = Configuration.get();
    bool performRestart = root["curPin"]["name"].as<String>() != config.Dev_PinMapping;

    strlcpy(config.Dev_PinMapping, root["curPin"]["name"].as<String>().c_str(), sizeof(config.Dev_PinMapping));
#ifdef USE_DISPLAY_GRAPHIC
    config.Display.Rotation = root["display"]["rotation"].as<uint8_t>();
    config.Display.PowerSafe = root["display"]["power_safe"].as<bool>();
    config.Display.ScreenSaver = root["display"]["screensaver"].as<bool>();
    config.Display.Contrast = root["display"]["contrast"].as<uint8_t>();
    config.Display.Language = root["display"]["language"].as<uint8_t>();
    config.Display.Diagram.Duration = root["display"]["diagramduration"].as<uint32_t>();
    config.Display.Diagram.Mode = root["display"]["diagrammode"].as<DiagramMode_t>();
#endif

#ifdef USE_LED_SINGLE
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        config.Led_Single[i].Brightness = root["led"][i]["brightness"].as<uint8_t>();
        config.Led_Single[i].Brightness = min<uint8_t>(100, config.Led_Single[i].Brightness);
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

    response->setLength();
    request->send(response);

    if (performRestart) {
        Utils::restartDtu();
    }
}
