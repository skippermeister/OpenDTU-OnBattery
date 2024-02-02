// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_DISPLAY_GRAPHIC

#pragma once

#include "Display_Graphic_Diagram.h"
#include "defaults.h"
#include <TaskSchedulerDeclarations.h>
#include <TimeoutHelper.h>
#include <U8g2lib.h>

#define CHART_HEIGHT 20 // chart area hight in pixels
#define CHART_WIDTH 47 // chart area width in pixels

// Left-Upper position of diagram is drawn
// (text of Y-axis is display left of that pos)
#define CHART_POSX 80
#define CHART_POSY 0

enum DisplayType_t {
    None,
    PCD8544_HW_SPI,
    PCD8544_SW_SPI,
    SSD1306,
    SH1106,
    SSD1309,
    ST7567_GM12864I_59N,
    ePaper154,
    DisplayType_Max,
};

enum DiagramMode_t {
    Off,
    Small,
    Fullscreen,
    DisplayMode_Max,
};

class DisplayGraphicClass {
public:
    DisplayGraphicClass();
    ~DisplayGraphicClass();

    void init(Scheduler& scheduler);
    void setContrast(const uint8_t contrast);
    void setStatus(const bool turnOn);
    void setOrientation(const uint8_t rotation = DISPLAY_ROTATION);
    void setLanguage(const uint8_t language);
    void setDiagramMode(DiagramMode_t mode);
    void setStartupDisplay();

#ifdef USE_DISPLAY_GRAPHIC_DIAGRAM
    DisplayGraphicDiagramClass& Diagram();
#endif

    bool enablePowerSafe = true;
    bool enableScreensaver = true;

private:
    void loop();

    void printText(const char* text, const uint8_t line);
    void calcLineHeights();
    void setFont(const uint8_t line);
    bool isValidDisplay();

    Task _loopTask;

    U8G2* _display;
#ifdef USE_DISPLAY_GRAPHIC_DIAGRAM
    DisplayGraphicDiagramClass _diagram;
#endif

    bool _displayTurnedOn;

    DisplayType_t _display_type = DisplayType_t::None;
    DiagramMode_t _diagram_mode = DiagramMode_t::Off;
    uint8_t _display_language = DISPLAY_LANGUAGE;
    uint8_t _mExtra;
    const uint16_t _period = 1000;
    const uint16_t _interval = 60000; // interval at which to power save (milliseconds)
    TimeoutHelper _previousMillis;
    char _fmtText[32];
    bool _isLarge = false;
    uint8_t _lineOffsets[5];
};

extern DisplayGraphicClass Display;

#endif
