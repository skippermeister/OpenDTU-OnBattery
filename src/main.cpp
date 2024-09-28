// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "Battery.h"
#include "Configuration.h"
#include "Datastore.h"
#include "Display_Graphic.h"
#include "InverterSettings.h"
#include "Led_Single.h"
#include "Led_Strip.h"
#include "MessageOutput.h"
#include "SerialPortManager.h"
#include "SPIPortManager.h"
#include "REFUsolRS485Receiver.h"
#include "VictronMppt.h"
#include "Huawei_can.h"
#include "MeanWell_can.h"
#include "ModbusDTU.h"
#include "MqttHandleDtu.h"
#include "MqttHandleHass.h"
#include "MqttHandleInverter.h"
#include "MqttHandleInverterTotal.h"
#include "MqttHandleBatteryHass.h"
#include "MqttHandleREFUsol.h"
#include "MqttHandleVedirect.h"
#include "MqttHandleVedirectHass.h"
#include "MqttHandleHuawei.h"
#include "MqttHandleMeanWell.h"
#include "MqttHandleMeanWellHass.h"
#include "MqttHandlePowerLimiter.h"
#include "MqttHandlePowerLimiterHass.h"
#include "MqttHandleZeroExport.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "NtpSettings.h"
#include "PinMapping.h"
#include "RestartHelper.h"
#include "PowerLimiter.h"
#include "PowerMeter.h"
#include "Scheduler.h"
#include "SunPosition.h"
#include "Utils.h"
#include "WebApi.h"
#include "ZeroExport.h"
#include "defaults.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <TaskScheduler.h>
#include <esp_heap_caps.h>

void setup()
{
    // Move all dynamic allocations >512byte to psram (if available)
    heap_caps_malloc_extmem_enable(512);

    // Initialize serial output
    Serial.begin(SERIAL_BAUDRATE);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
    delay(5000);
#else
    while (!Serial)
        yield();
#endif

    MessageOutput.init(scheduler);
    MessageOutput.println("\r\nStarting OpenDTU-onBattery");

    // Initialize file system
    MessageOutput.print("Initialize FS... ");
    if (!LittleFS.begin(false)) { // Do not format if mount failed
        MessageOutput.print("failed... trying to format...");
        if (!LittleFS.begin(true)) {
            MessageOutput.print("success");
        } else {
            MessageOutput.print("failed");
        }
    } else {
        MessageOutput.println("done");
    }

    // Read configuration values
    MessageOutput.print("Reading configuration... ");
    if (!Configuration.read()) {
        MessageOutput.print("initializing... ");
        Configuration.init();
        if (Configuration.write()) {
            MessageOutput.print("written... ");
        } else {
            MessageOutput.print("failed... ");
        }
    }
    CONFIG_T& config = Configuration.get();
    if (config.Cfg.Version != CONFIG_VERSION) {
        MessageOutput.print("migrated... ");
        Configuration.migrate();
    }
    MessageOutput.println("done");

    PinMapping.init(String(config.Dev_PinMapping)); // Load PinMapping

    SerialPortManager.init();
    SPIPortManager.init();

    // Initialize WiFi
    NetworkSettings.init(scheduler);
    NetworkSettings.applyConfig();
    vTaskDelay(1000); // FIXME

    NtpSettings.init(); // Initialize NTP

    SunPosition.init(scheduler); // Initialize SunPosition

    // Initialize MqTT
    MessageOutput.print("Initialize MqTT... ");
    MqttSettings.init();
    MqttHandleDtu.init(scheduler);
    MqttHandleInverter.init(scheduler);
    MqttHandleInverterTotal.init(scheduler);
    MqttHandleVedirect.init(scheduler);
#ifdef USE_REFUsol_INVERTER
    MqttHandleREFUsol.init(scheduler);
#endif

#ifdef USE_HASS
    MqttHandleHass.init(scheduler);
    MqttHandleVedirectHass.init(scheduler);
    MqttHandleBatteryHass.init(scheduler);
    MqttHandlePowerLimiterHass.init(scheduler);
#ifdef USE_CHARGER_MEANWELL
    MqttHandleMeanWellHass.init(scheduler);
#endif
#endif

#ifdef USE_CHARGER_HUAWEI
    MqttHandleHuawei.init(scheduler);
#endif
#ifdef USE_CHARGER_MEANWELL
    MqttHandleMeanWell.init(scheduler);
#endif
    MqttHandlePowerLimiter.init(scheduler);
    MqttHandleZeroExport.init(scheduler);
    MessageOutput.println("done");

    WebApi.init(scheduler); // Initialize WebApi

#ifdef USE_DISPLAY_GRAPHIC
    Display.init(scheduler); // Initialize Display
#endif

#ifdef USE_LED_SINGLE
    LedSingle.init(scheduler); // Initialize Single LEDs
#endif
#ifdef USE_LED_STRIP
    // Initialize LED WS2812
    LedStrip.init(scheduler);
#endif

    // Check for default DTU serial
    MessageOutput.print("Check for default DTU serial... ");
    if (config.Dtu.Serial == DTU_SERIAL) {
        MessageOutput.print("generate serial based on ESP chip id: ");
        uint64_t dtuId = Utils::generateDtuSerial();
        MessageOutput.printf("%0x%08x... ",
            ((uint32_t)((dtuId >> 32) & 0xFFFFFFFF)),
            ((uint32_t)(dtuId & 0xFFFFFFFF)));
        config.Dtu.Serial = dtuId;
        Configuration.write();
    }
    MessageOutput.println("done");

    InverterSettings.init(scheduler);

    Datastore.init(scheduler);
    RestartHelper.init(scheduler);

    VictronMppt.init(scheduler); // Initialize ve.direct communication
#ifdef USE_REFUsol_INVERTER
    REFUsol.init(scheduler); // Initialize REFUsol communication
#endif
    PowerMeter.init(scheduler); // Power meter
    PowerLimiter.init(scheduler); // Dynamic power limiter
    ZeroExport.init(scheduler); // Dynamic Zero Eport limiter

#ifdef USE_CHARGER_HUAWEI
    HuaweiCan.init(scheduler); // Initialize Huawei AC-charger PSU / CAN bus
#endif
#ifdef USE_CHARGER_MEANWELL
    MeanWellCan.init(scheduler); // Initialize MeanWell NPB-1200-48 AC-charger PSU / CAN bus
#endif

    Battery.init(scheduler);

#ifdef USE_ModbusDTU
    ModbusDtu.init(scheduler);
#endif

    MessageOutput.printf("Free heap: %d\r\n", xPortGetFreeHeapSize());
}

void loop()
{
    scheduler.execute();
}
