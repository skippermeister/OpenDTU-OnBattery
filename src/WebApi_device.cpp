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
    if (!WebApi.checkCredentialsReadonly(request)) {
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

#if defined(USE_RADIO_NRF)
    auto nrfPinObj = curPin.createNestedObject("nrf24");
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
    auto cmtPinObj = curPin.createNestedObject("cmt");
    if (PinMapping.isValidCmt2300Config()) {
        cmtPinObj["clk"] = pin.cmt_clk;
        cmtPinObj["cs"] = pin.cmt_cs;
        cmtPinObj["fcs"] = pin.cmt_fcs;
        cmtPinObj["sdio"] = pin.cmt_sdio;
        cmtPinObj["gpio2"] = pin.cmt_gpio2;
        cmtPinObj["gpio3"] = pin.cmt_gpio3;
    } else {
        cmtPinObj["Pins"] = "invalid";
    }
#endif

#if defined(OPENDTU_ETHERNET)
    auto ethPinObj = curPin.createNestedObject("eth");
    ethPinObj["enabled"] = pin.eth_enabled;
    ethPinObj["phy_addr"] = pin.eth_phy_addr;
    ethPinObj["power"] = pin.eth_power;
    ethPinObj["mdc"] = pin.eth_mdc;
    ethPinObj["mdio"] = pin.eth_mdio;
    ethPinObj["type"] = pin.eth_type;
    ethPinObj["clk_mode"] = pin.eth_clk_mode;
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    auto displayPinObj = curPin.createNestedObject("display");
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
    auto ledPinObj = curPin.createNestedObject("led");
#if defined(USE_LED_SINGLE)
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        ledPinObj["led" + String(i)] = pin.led[i];
    }
#endif
#if defined(USE_LED_STRIP)
    ledPinObj["rgb"] = pin.led_rgb;
#endif
    auto leds = root.createNestedArray("led");
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        JsonObject led = leds.createNestedObject();
        led["brightness"] = config.Led[i].Brightness;
    }
#endif

#if defined(USE_DISPLAY_GRAPHIC)
    auto display = root.createNestedObject("display");
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

    auto victronPinObj = curPin.createNestedObject("victron");
    victronPinObj["rs232_rx"] = pin.victron_rx;
    victronPinObj["rs232_tx"] = pin.victron_tx;
    if (pin.victron_rx2 > 0 && pin.victron_tx2 >= 0) {
        victronPinObj["rs232_rx2"] = pin.victron_rx2;
        victronPinObj["rs232_tx2"] = pin.victron_tx2;
    }

#if defined(USE_REFUsol_INVERTER)
    auto refusolPinObj = curPin.createNestedObject("refusol");
    refusolPinObj["rs485_rx"] = pin.REFUsol_rx;
    refusolPinObj["rs485_tx"] = pin.REFUsol_tx;
    if (pin.REFUsol_rts >= 0) {
        refusolPinObj["rs485_rts"] = pin.REFUsol_rts;
    }
#endif

    auto batteryPinObj = curPin.createNestedObject("battery");
#if defined(USE_PYLONTECH_RS485_RECEIVER) || defined(USE_DALYBMS_CONTROLLER) || defined(USE_JKBMS_CONTROLLER)
    if (pin.battery_rts >= -1) {
        batteryPinObj["rs485_rx"] = pin.battery_rx;
        batteryPinObj["rs485_tx"] = pin.battery_tx;
        if (pin.battery_rts >= 0) {
            batteryPinObj["rs485_rts"] = pin.battery_rts;
        }
    } else {
        batteryPinObj["rs232_rx"] = pin.battery_rx;
        batteryPinObj["rs232_tx"] = pin.battery_tx;
    }
#if defined(USE_DALYBMS_CONTROLLER)
    if (pin.battery_bms_wakeup >= 0) batteryPinObj["bms_wakeup"] = pin.battery_bms_wakeup;
#endif
#else
    batteryPinObj["can0_rx"] = pin.battery_rx;
    batteryPinObj["can0_tx"] = pin.battery_tx;
#endif

    auto chargerPinObj = curPin.createNestedObject("charger");
#if defined(CHARGER_HUAWEI)
    chargerPinObj["power"] = pin.huawei_power;
#endif
#if defined(CHARGER_USE_CAN0)
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

    if (static_cast<PowerMeterClass::Source>(config.PowerMeter.Source) == PowerMeterClass::Source::SML)
    {
        auto smlPinObj = curPin.createNestedObject("powermeter");
        smlPinObj["sml_rs232_rx"] = pin.powermeter_rx;
        if (pin.powermeter_tx >= 0) smlPinObj["sml_rs232_tx"] = pin.powermeter_tx;
    } else if (   static_cast<PowerMeterClass::Source>(config.PowerMeter.Source) == PowerMeterClass::Source::SDM1PH
               || static_cast<PowerMeterClass::Source>(config.PowerMeter.Source) == PowerMeterClass::Source::SDM3PH)
    {
        auto sdmPinObj = curPin.createNestedObject("powermeter");
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

    if (!(root.containsKey("curPin")
#if defined(USE_DISPLAY_GRAPHIC)
    || root.containsKey("display")
#endif
        )) {
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

    response->setLength();
    request->send(response);

    if (performRestart) {
        Utils::restartDtu();
    }
}
