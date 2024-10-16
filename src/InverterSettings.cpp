// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Thomas Basler and others
 */
#include "InverterSettings.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "SunPosition.h"
#include <Hoymiles.h>
#include <SpiManager.h>

InverterSettingsClass InverterSettings;

InverterSettingsClass::InverterSettingsClass()
    : _settingsTask(INVERTER_UPDATE_SETTINGS_INTERVAL, TASK_FOREVER, std::bind(&InverterSettingsClass::settingsLoop, this))
    , _hoyTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&InverterSettingsClass::hoyLoop, this))
{
}

void InverterSettingsClass::init(Scheduler& scheduler)
{
    MessageOutput.print("initialize Hoymiles interface... ");

    Hoymiles.setMessageOutput(&MessageOutput);
    Hoymiles.init();

    const CONFIG_T& config = Configuration.get();
    const PinMapping_t& pin = PinMapping.get();

    // Initialize inverter communication
    if (
#ifdef USE_RADIO_NRF
        PinMapping.isValidNrf24Config()
#endif
#if defined(USE_RADIO_NRF) && defined(USE_RADIO_CMT)
        ||
#endif
#ifdef USE_RADIO_CMT
        PinMapping.isValidCmt2300Config()
#endif
      )
    {
#ifdef USE_RADIO_NRF
        if (PinMapping.isValidNrf24Config()) {
            auto spi_bus = SpiManagerInst.claim_bus_arduino();
            ESP_ERROR_CHECK(spi_bus ? ESP_OK : ESP_FAIL);

            MessageOutput.printf("Init NRF24 chip: CLK: %d, MISO: %d, MOSI: %d, CS: %d, EN: %d, IRQ: %d\r\n",
                pin.nrf24_clk, pin.nrf24_miso, pin.nrf24_mosi, pin.nrf24_cs, pin.nrf24_en, pin.nrf24_irq);

            SPIClass* spiClass = new SPIClass(*spi_bus);
            spiClass->begin(pin.nrf24_clk, pin.nrf24_miso, pin.nrf24_mosi, pin.nrf24_cs);
            Hoymiles.initNRF(spiClass, pin.nrf24_en, pin.nrf24_irq);
        }
#endif
#ifdef USE_RADIO_CMT
        if (PinMapping.isValidCmt2300Config()) {
            MessageOutput.printf("Init CMT2300A chip: SDIO: %d, CLK: %d, CS: %d, FCS: %d, GPIO2: %d, GPIO3: %d, Chip Int1@GPIO: %d, Int2@GPIO: %d\r\n",
                pin.cmt_sdio, pin.cmt_clk, pin.cmt_cs, pin.cmt_fcs,
                pin.cmt_gpio2, pin.cmt_gpio3,
                pin.cmt_chip_int1gpio, pin.cmt_chip_int2gpio);

            Hoymiles.initCMT(pin.cmt_sdio, pin.cmt_clk, pin.cmt_cs, pin.cmt_fcs, pin.cmt_gpio2, pin.cmt_gpio3,
                pin.cmt_chip_int1gpio, pin.cmt_chip_int2gpio);
            MessageOutput.println("  Setting country mode... ");
            Hoymiles.getRadioCmt()->setCountryMode(static_cast<CountryModeId_t>(config.Dtu.Cmt.CountryMode));
            MessageOutput.println("  Setting CMT target frequency... ");
            Hoymiles.getRadioCmt()->setInverterTargetFrequency(config.Dtu.Cmt.Frequency);
        }
#endif
        MessageOutput.println("  Setting radio PA level... ");
        Hoymiles.getRadioNrf()->setPALevel((rf24_pa_dbm_e)config.Dtu.Nrf.PaLevel);
#ifdef USE_RADIO_CMT
        Hoymiles.getRadioCmt()->setPALevel(config.Dtu.Cmt.PaLevel);
#endif
        MessageOutput.println("  Setting DTU serial... ");
        Hoymiles.getRadioNrf()->setDtuSerial(config.Dtu.Serial);
#ifdef USE_RADIO_CMT
        Hoymiles.getRadioCmt()->setDtuSerial(config.Dtu.Serial);
#endif
        MessageOutput.println("  Setting poll interval... ");
        Hoymiles.setPollInterval(config.Dtu.PollInterval);

        for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
            if (config.Inverter[i].Serial > 0) {
                MessageOutput.print("  Adding inverter: ");
                MessageOutput.print(config.Inverter[i].Serial, HEX);
                MessageOutput.print(" - ");
                MessageOutput.print(config.Inverter[i].Name);
                auto inv = Hoymiles.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial);

                if (inv != nullptr) {
                    inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
                    inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
                    inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
                    inv->setClearEventlogOnMidnight(config.Inverter[i].ClearEventlogOnMidnight);
                    inv->Statistics()->setYieldDayCorrection(config.Inverter[i].YieldDayCorrection);
                    for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                        inv->Statistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
                        inv->Statistics()->setChannelFieldOffset(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_YT, config.Inverter[i].channel[c].YieldTotalOffset);
                    }
                }
                MessageOutput.println(" done");
            }
        }
        MessageOutput.println("done");
    } else {
        MessageOutput.println("Invalid pin config");
    }

    scheduler.addTask(_hoyTask);
    _hoyTask.enable();

    scheduler.addTask(_settingsTask);
    _settingsTask.enable();
}

void InverterSettingsClass::settingsLoop()
{
    const CONFIG_T& config = Configuration.get();
    const boolean isDayPeriod = SunPosition.isDayPeriod();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        auto const& inv_cfg = config.Inverter[i];
        if (inv_cfg.Serial == 0) {
            continue;
        }
        auto inv = Hoymiles.getInverterBySerial(inv_cfg.Serial);
        if (inv == nullptr) {
            continue;
        }

        inv->setEnablePolling((inv_cfg.Poll_Enable_Day && isDayPeriod) || (!isDayPeriod && inv_cfg.Poll_Enable_Night));
        inv->setEnableCommands((inv_cfg.Command_Enable_Day && isDayPeriod) || (!isDayPeriod && inv_cfg.Command_Enable_Night));
    }
}

void InverterSettingsClass::hoyLoop()
{
    Hoymiles.loop();
}
