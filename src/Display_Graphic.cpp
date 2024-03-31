// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023-2024 Thomas Basler and others
 */
#ifdef USE_DISPLAY_GRAPHIC

#include "Display_Graphic.h"
#include "Configuration.h"
#include "Datastore.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "PowerMeter.h"
#include <NetworkSettings.h>
#include <map>
#include <time.h>

std::map<DisplayType_t, std::function<U8G2*(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t)>> display_types = {
    { DisplayType_t::PCD8544_HW_SPI, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_PCD8544_84X48_F_4W_HW_SPI(U8G2_R0, cs, data, reset); } },
    { DisplayType_t::PCD8544_SW_SPI, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_PCD8544_84X48_F_4W_SW_SPI(U8G2_R2, clock, data, cs, dc, reset); } },
    { DisplayType_t::SSD1306, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, reset, clock, data); } },
    { DisplayType_t::SH1106, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, reset, clock, data); } },
    { DisplayType_t::SSD1309, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0, reset, clock, data); } },
    { DisplayType_t::ST7567_GM12864I_59N, [](uint8_t reset, uint8_t clock, uint8_t data, uint8_t cs, uint8_t dc) { return new U8G2_ST7567_ENH_DG128064I_F_HW_I2C(U8G2_R0, reset, clock, data); } },
};

// Language defintion, respect order in languages[] and translation lists
#define I18N_LOCALE_EN 0
#define I18N_LOCALE_DE 1
#define I18N_LOCALE_FR 2

// Languages supported. Note: the order is important and must match locale_translations.h
const uint8_t languages[] = {
    I18N_LOCALE_EN,
    I18N_LOCALE_DE,
    I18N_LOCALE_FR
};

static const char* const i18n_offline[] = { "Offline", "Offline", "Offline" };

static const char* const i18n_current_power_w[] = { "%.0f W", "%.0f W", "%.0f W" };
static const char* const i18n_current_power_kw[] = { "%.1f kW", "%.1f kW", "%.1f kW" };

static const char* const i18n_meter_power_w[] = { "grid: %.0f W", "Netz: %.0f W", "reseau: %.0f W" };
static const char* const i18n_meter_power_kw[] = { "grid: %.1f kW", "Netz: %.1f kW", "reseau: %.1f kW" };

static const char* const i18n_yield_today_wh[] = { "today: %4.0f Wh", "Heute: %4.0f Wh", "auj.: %4.0f Wh" };
static const char* const i18n_yield_today_kwh[] = { "today: %.1f kWh", "Heute: %.1f kWh", "auj.: %.1f kWh" };

static const char* const i18n_yield_total_kwh[] = { "total: %.1f kWh", "Ges.: %.1f kWh", "total: %.1f kWh" };
static const char* const i18n_yield_total_mwh[] = { "total: %.0f kWh", "Ges.: %.0f kWh", "total: %.0f kWh" };

static const char* const i18n_date_format[] = { "%m/%d/%Y %H:%M", "%d.%m.%Y %H:%M", "%d/%m/%Y %H:%M" };

DisplayGraphicClass::DisplayGraphicClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&DisplayGraphicClass::loop, this))
{
}

DisplayGraphicClass::~DisplayGraphicClass()
{
    delete _display;
}

void DisplayGraphicClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Display... ");

    _previousMillis.set(_interval * 2);

    const PinMapping_t& pin = PinMapping.get();
    _display_type = static_cast<DisplayType_t>(pin.display_type);
    if (isValidDisplay()) {
        auto constructor = display_types[_display_type];
        _display = constructor(
            pin.display_reset < 0 ? 255 : pin.display_reset,
            pin.display_clk < 0 ? 255 : pin.display_clk,
            pin.display_data < 0 ? 255 : pin.display_data,
            pin.display_cs < 0 ? 255 : pin.display_cs,
            pin.display_cs < 0 ? 255 : pin.display_dc);
        if (_display_type == DisplayType_t::ST7567_GM12864I_59N) {
            _display->setI2CAddress(0x3F << 1);
        }
        _display->begin();
        setContrast(DISPLAY_CONTRAST);
        setStatus(true);
        _diagram.init(scheduler, _display);

        scheduler.addTask(_loopTask);
        _loopTask.setInterval(_period);
        _loopTask.enable();

    } else {
        MessageOutput.println("none configured. ");
        return;
    }

    const Display_CONFIG_T& cDisplay = Configuration.get().Display;
    setDiagramMode(static_cast<DiagramMode_t>(cDisplay.Diagram.Mode));
    setOrientation(cDisplay.Rotation);
    enablePowerSafe = cDisplay.PowerSafe;
    enableScreensaver = cDisplay.ScreenSaver;
    setContrast(cDisplay.Contrast);
    setLanguage(cDisplay.Language);
    setStartupDisplay();

    MessageOutput.println("done");
}

void DisplayGraphicClass::calcLineHeights()
{
    bool diagram = (_isLarge && _diagram_mode == DiagramMode_t::Small);
    // the diagram needs space. we need to keep
    // away from the y-axis label in particular.
    uint8_t yOff = (diagram ? 7 : 0);
    for (uint8_t i = 0; i < 4; i++) {
        setFont(i);
        yOff += _display->getAscent();
        _lineOffsets[i] = yOff;
        yOff += ((!_isLarge || diagram) ? 2 : 3);
        // the descent is a negative value and moves the *next* line's
        // baseline. the first line never uses a letter with descent and
        // we need that space when showing the small diagram.
        yOff -= ((i == 0 && diagram) ? 0 : _display->getDescent());
    }
}

void DisplayGraphicClass::setFont(const uint8_t line)
{
    switch (line) {
    case 0:
        _display->setFont((_isLarge) ? u8g2_font_ncenB14_tr : u8g2_font_logisoso16_tr);
        //        _display->setFont((_isLarge) ? u8g2_font_ncenB12_tr : u8g2_font_logisoso16_tr);
        // Daten visualisieren #168
        //_display->setFont((_isLarge) ? u8g2_font_ncenB10_tr : u8g2_font_ncenB10_tr);
        break;
    case 3:
        _display->setFont(u8g2_font_5x8_tr);
        break;
    default:
        _display->setFont((_isLarge) ? u8g2_font_ncenB10_tr : u8g2_font_5x8_tr);
        break;
    }
}

bool DisplayGraphicClass::isValidDisplay()
{
    return _display_type > DisplayType_t::None && _display_type < DisplayType_Max;
}

void DisplayGraphicClass::printText(const char* text, const uint8_t line)
{
    setFont(line);

    uint8_t dispX;
    if (!_isLarge) {
        dispX = (line == 0) ? 5 : 0;
    } else {
        if (line == 0 && _diagram_mode == DiagramMode_t::Small) {
            // Center between left border and diagram
            dispX = (CHART_POSX - _display->getStrWidth(text)) / 2;
        } else {
            // Center on screen
            dispX = (_display->getDisplayWidth() - _display->getStrWidth(text)) / 2;
        }
    }

    if (enableScreensaver) {
        unsigned maxOffset = (_isLarge ? 8 : 6);
        unsigned period = 2 * maxOffset;
        unsigned step = _mExtra % period;
        int offset = (step <= maxOffset) ? step : (period - step);
        offset -= (_isLarge ? 5 : 0); // oscillate around center on large screens
        dispX += offset;
    }

    if (dispX > _display->getDisplayWidth()) {
        dispX = 0;
    }
    _display->drawStr(dispX, _lineOffsets[line], text);
}

void DisplayGraphicClass::setDiagramMode(DiagramMode_t mode)
{
    if (mode < DiagramMode_t::DisplayMode_Max) {
        _diagram_mode = mode;
    }
}

void DisplayGraphicClass::setOrientation(const uint8_t rotation)
{
    if (_display_type == DisplayType_t::None) {
        return;
    }

    switch (rotation) {
    case 0:
        _display->setDisplayRotation(U8G2_R0);
        break;
    case 1:
        _display->setDisplayRotation(U8G2_R1);
        break;
    case 2:
        _display->setDisplayRotation(U8G2_R2);
        break;
    case 3:
        _display->setDisplayRotation(U8G2_R3);
        break;
    }

    _isLarge = (_display->getWidth() > 100);
    calcLineHeights();
}

void DisplayGraphicClass::setLanguage(const uint8_t language)
{
    _display_language = language < sizeof(languages) / sizeof(languages[0]) ? language : DISPLAY_LANGUAGE;
}

void DisplayGraphicClass::setStartupDisplay()
{
    if (!isValidDisplay()) {
        return;
    }

    _display->clearBuffer();
    printText("OpenDTU!", 0);
    _display->sendBuffer();
}

DisplayGraphicDiagramClass& DisplayGraphicClass::Diagram()
{
    return _diagram;
}

void DisplayGraphicClass::loop()
{
    _loopTask.setInterval(_period);

    _display->clearBuffer();
    bool displayPowerSave = false;
    bool showText = true;

    //=====> Actual Production ==========
    if (Datastore.getIsAtLeastOneReachable()) {
        displayPowerSave = false;
        if (_isLarge) {
            uint8_t screenSaverOffsetX = enableScreensaver ? (_mExtra % 7) : 0;
            switch (_diagram_mode) {
            case DiagramMode_t::Small:
                _diagram.redraw(screenSaverOffsetX, CHART_POSX, CHART_POSY, CHART_WIDTH, CHART_HEIGHT, false);
                break;
            case DiagramMode_t::Fullscreen:
                // Every 10 seconds
                if (_mExtra % (10 * 2) < 10) {
                    _diagram.redraw(screenSaverOffsetX, 10, 0, _display->getDisplayWidth() - 12, _display->getDisplayHeight() - 3, true);
                    showText = false;
                }
                break;
            default:
                break;
            }
        }

        if (showText) {
            const float watts = Datastore.getTotalAcPowerEnabled();
            if (watts > 999) {
                snprintf(_fmtText, sizeof(_fmtText), i18n_current_power_kw[_display_language], watts / 1000);
            } else {
                snprintf(_fmtText, sizeof(_fmtText), i18n_current_power_w[_display_language], watts);
            }

            printText(_fmtText, 0);
        }

        _previousMillis.reset();
    }
    //<=======================

    //=====> Offline ===========
    else {
        printText(i18n_offline[_display_language], 0);
        // check if it's time to enter power saving mode
        if (_previousMillis.occured()) {
            displayPowerSave = enablePowerSafe;
        }
    }
    //<=======================

    //=====> Today & Total Production =======
    if (showText) {
        //=====> Today & Total Production =======
        // Daily production
        float wattsToday = Datastore.getTotalAcYieldDayEnabled();
        if (wattsToday >= 10000) {
            snprintf(_fmtText, sizeof(_fmtText), i18n_yield_today_kwh[_display_language], wattsToday / 1000);
        } else {
            snprintf(_fmtText, sizeof(_fmtText), i18n_yield_today_wh[_display_language], wattsToday);
        }
        printText(_fmtText, 1);

        // Total production
        const float wattsTotal = Datastore.getTotalAcYieldTotalEnabled();
        auto const format = (wattsTotal >= 1000) ? i18n_yield_total_mwh : i18n_yield_total_kwh;
        snprintf(_fmtText, sizeof(_fmtText), format[_display_language], wattsTotal);
        printText(_fmtText, 2);

        //=====> IP or Date-Time ========
        // Change every 3 seconds
        if (!(_mExtra % (3 * 2) < 3) && NetworkSettings.localIP()) {
            printText(NetworkSettings.localIP().toString().c_str(), 3);
        } else {
            // Get current time
            time_t now = time(nullptr);
            strftime(_fmtText, sizeof(_fmtText), i18n_date_format[_display_language], localtime(&now));
            printText(_fmtText, 3);
        }
    }

    // the IP and time info in the third line use three-second slots. the
    // timing for the power meter is chosen such that every third of those
    // three-second slots is used to NOT overwrite the total inverter energy.
    bool timing = (_mExtra % 9) >= 3;

    if (showText && Configuration.get().PowerMeter.Enabled && timing && !displayPowerSave) {
        // erase the third line and print the power meter value instead.
        // we do it this way to touch as least upstream code as possible
        // to make maintenance easier.
        setFont(2);
        auto lineHeight = _display->getAscent() - _display->getDescent();
        auto y = _lineOffsets[2] - _display->getAscent();
        _display->setDrawColor(0);
        _display->drawBox(0, y, _display->getDisplayWidth(), lineHeight);
        _display->setDrawColor(1);

        auto acPower = PowerMeter.getPowerTotal(false);
        if (acPower > 999) {
            snprintf(_fmtText, sizeof(_fmtText), i18n_meter_power_kw[_display_language], (acPower / 1000));
        } else {
            snprintf(_fmtText, sizeof(_fmtText), i18n_meter_power_w[_display_language], acPower);
        }

        printText(_fmtText, 2);
    }

    _display->sendBuffer();

    _mExtra++;

    if (!_displayTurnedOn) {
        displayPowerSave = true;
    }

    _display->setPowerSave(displayPowerSave);
}

void DisplayGraphicClass::setContrast(const uint8_t contrast)
{
    if (!isValidDisplay()) {
        return;
    }
    _display->setContrast(contrast * 2.55f);
}

void DisplayGraphicClass::setStatus(const bool turnOn)
{
    _displayTurnedOn = turnOn;
}

DisplayGraphicClass Display;

#endif
