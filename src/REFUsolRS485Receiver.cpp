/* framehandler.cpp
 *
 * Arduino library to read from REFUsol devices using RS485 USS protocol.
 * Derived from Siemens USS framehandler reference implementation.
 *
 * The MIT License
 *
 */

#ifdef USE_REFUsol_INVERTER

static constexpr char TAG[] = "[REFUsol]";

#include "REFUsolRS485Receiver.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <driver/uart.h>

HardwareSerial REFUsolSerial(2);

REFUsolRS485ReceiverClass REFUsol;

REFUsolRS485ReceiverClass::REFUsolRS485ReceiverClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&REFUsolRS485ReceiverClass::loop, this))
    , _lastPoll(0)
{
}

void REFUsolRS485ReceiverClass::init(Scheduler& scheduler)
{
    if (!Configuration.get().REFUsol.Enabled)
        return;

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("Initialize REFUsol RS485 interface RS485 port rx = %d, tx = %d rts = %d. ", pin.REFUsol_rx, pin.REFUsol_tx, pin.REFUsol_rts);

    if (!PinMapping.isValidREFUsolConfig()) {
        MessageOutput.println("Invalid pin config");
        return;
    }

    if (!_initialized) {
        RS485BaudRate = 57600;

        REFUsolSerial.begin(RS485BaudRate, SERIAL_8N1, pin.REFUsol_rx, pin.REFUsol_tx);
        REFUsolSerial.flush();
        REFUsolSerial.setPins(pin.REFUsol_rx, pin.REFUsol_tx, UART_PIN_NO_CHANGE, pin.REFUsol_rts);
        ESP_ERROR_CHECK(uart_set_mode(2, UART_MODE_RS485_HALF_DUPLEX));

        // Set read timeout of UART TOUT feature
        ESP_ERROR_CHECK(uart_set_rx_timeout(2, ECHO_READ_TOUT));

        MessageOutput.print("initialized successfully. ");
        _initialized = true;
    }

    MessageOutput.print("Read basic infos and parameters. ");

    getPVpeak(adr);
    parse();

    getPVlimit(adr);
    parse();

    getDeviceSpecificOffset(adr);
    parse();

    getPlantSpecificOffset(adr);
    parse();

    getOptionCosPhi(adr);
    parse();
    Frame.effectivCosPhi = 0.0;
    switch (Frame.optionCosPhi) {
    case 0:
        Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset;
        break;
    case 1:
        getFixedOffset(adr);
        parse();
        if (Frame.fixedOffset > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.fixedOffset;
        break;
    case 2:
        getVariableOffset(adr);
        parse();
        if (Frame.variableOffset > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.variableOffset;
        break;
    case 3:
        // to be done
        Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.cosPhi;
        break;
    case 4:
        getCosPhi(adr);
        parse();
        if (Frame.cosPhi > NaN) {
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.cosPhi;
        }
        break;
    default:;
    }
    if (Frame.optionCosPhi >= 0)
        MessageOutput.printf("%s Effektiver Cos Phi : %.2f°\r\n", TAG, Frame.effectivCosPhi);

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    _isInstalled = true;

    MessageOutput.println("done");
}

void REFUsolRS485ReceiverClass::deinit(void)
{
    if (!_isInstalled) {
        return;
    }

    MessageOutput.printf("%s RS485 driver uninstalled\r\n", TAG);
    _isInstalled = false;
}

void REFUsolRS485ReceiverClass::loop()
{
    REFUsol_CONFIG_T& cREFUsol = Configuration.get().REFUsol;

    if (!cREFUsol.Enabled
        || (millis() - getLastUpdate()) < cREFUsol.PollInterval * 1000) {
        if (!cREFUsol.Enabled)
            deinit();
        return;
    }

    if (!_isInstalled) {
        init();
        return;
    }

    if (REFUsolSerial.available()) {
        parse();
    }

    static int state = 0;
    switch (state) {
    case 0:
    case 5:
        MessageOutput.printf("%s DC Spannungen\r\n", TAG);
        getDCpower(adr);
        parse();

        getDCcurrent(adr);
        parse();

        getDCvoltage(adr);
        parse();

        getACpower(adr);
        parse();

        state++;
        break;

    case 1:
    case 6:
        MessageOutput.printf("%s AC Spannungen\r\n", TAG);
        getACvoltage(adr, 0);
        parse();
        getACvoltage(adr, 1);
        parse();
        getACvoltage(adr, 2);
        parse();
        getACvoltage(adr, 3);
        parse();

        state++;
        break;

    case 2:
    case 7:
        MessageOutput.printf("%s AC Ströme\r\n", TAG);
        getACcurrent(adr, 0);
        parse();
        getACcurrent(adr, 1);
        parse();
        getACcurrent(adr, 2);
        parse();
        getACcurrent(adr, 3);
        parse();

        state++;
        break;

    case 3:
    case 8:
        MessageOutput.printf("%s Phasen Frequenzen\r\n", TAG);
        getACfrequency(adr, 1);
        parse();
        getACfrequency(adr, 2);
        parse();
        getACfrequency(adr, 3);
        parse();

        state++;
        break;

    case 4:
    case 9:
        MessageOutput.printf("%s Yield Solar Power\r\n", TAG);
        getYieldDay(adr);
        parse();
        getYieldMonth(adr);
        parse();
        getYieldYear(adr);
        parse();
        getYieldTotal(adr);
        parse();

        getTotalOperatingHours(adr);
        parse();

        state++;
        break;

    case 10:
        getTemperatursensor(adr, 1);
        parse();
        getTemperatursensor(adr, 2);
        parse();
        getTemperatursensor(adr, 3);
        parse();
        getTemperatursensor(adr, 4);
        parse();

        state++;
        break;
    }

    setLastUpdate();
}

void REFUsolRS485ReceiverClass::debugRawTelegram(TELEGRAM_t* t, uint8_t len)
{
    char buffer[1024];
    if (DebugRawTelegram) {
        for (uint8_t i = 0; i < len; i++) {
            snprintf(&buffer[i * 3], 4, "%02X ", t->buffer[i]);
        }
        MessageOutput.println(buffer);
    }
}

float REFUsolRS485ReceiverClass::PWE2Float(float scale)
{
    return static_cast<float>((rTelegram.PWE1.H << 24) + (rTelegram.PWE1.L << 16) + (rTelegram.PWE2.H << 8) + (rTelegram.PWE2.L)) * scale;
}

uint32_t REFUsolRS485ReceiverClass::PWE2Uint32(void)
{
    return (rTelegram.PWE1.H << 24) + (rTelegram.PWE1.L << 16) + (rTelegram.PWE2.H << 8) + (rTelegram.PWE2.L);
}

void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND)
{
    compute(adr, mirror, TaskID, PNU, IND, reinterpret_cast<void*>(NULL), PZD_ANZ,  reinterpret_cast<void*>(NULL));
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE)
{
    compute(adr, mirror, TaskID, PNU, IND, PWE, PZD_ANZ, reinterpret_cast<void*>(NULL));
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, void* PZD)
{
    compute(adr, mirror, TaskID, PNU, IND, PWE, PZD_ANZ, PZD);
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD)
{
    compute(adr, mirror, TaskID, PNU, IND, PWE, pzd_anz, PZD);
}

void REFUsolRS485ReceiverClass::compute(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD)
{
    uint8_t bcc = 0;
    uint8_t lge = 0;

    adr &= 0b00011111;
    sTelegram.STX = 0x02;
    sTelegram.ADR = adr | (mirror ? 0b01000000 : 0b00000000);
    lge += 1;
    lge += 1;

    sTelegram.PKE.H = (PNU | (TaskID & 0xF) << 12) >> 8;
    sTelegram.PKE.L = (PNU | (TaskID & 0xF) << 12) & 0xFF;
    lge += 2;

    sTelegram.IND.H = (IND | (TaskID & 0x100)) >> 8;
    sTelegram.IND.L = (IND | (TaskID & 0x100)) & 0xFF;
    lge += 2;

    switch (TaskID) {
    case 0b0000:
    case 0b0001:
    case 0b0010:
    case 0b0110:
    case 0b0111:
    case 0b1001:
    case 0b1100:
    case 0b1110:
        if (PKW_ANZ > 3) {
            sTelegram.buffer[++lge] = 0;
            sTelegram.buffer[++lge] = 0;
        }
        if ((TaskID == 0b0001) || (TaskID == 0b0001) || (TaskID == 0b0110) || (TaskID == 0b1001)) {
            sTelegram.buffer[++lge] = 0;
            sTelegram.buffer[++lge] = 0;
        } else {
            sTelegram.buffer[++lge] = (*(reinterpret_cast<uint16_t*>(PWE))) >> 8;
            sTelegram.buffer[++lge] = (*(reinterpret_cast<uint16_t*>(PWE))) & 0xFF;
        }
        break;

    case 0b0011:
    case 0b1000: // ChangePWEarray
    case 0b1101:
        sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PWE))) >> 24;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PWE))) >> 16;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PWE))) >> 8;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PWE))) & 0xFF;
        break;

    case 0b100001111: {
        bool flag = false;
        uint8_t* s =  reinterpret_cast<uint8_t*>(PWE);
        for (int i = 0; i < 16; i++) {
            if (*s == 0)
                flag = true;
            if (!flag)
                sTelegram.buffer[++lge] = *s++;
            else
                sTelegram.buffer[++lge] = 0x20;
        }
    } break;
    case 0b000001111:
        sTelegram.buffer[++lge] = 0;
        sTelegram.buffer[++lge] = 0;
        if (PKW_ANZ == 4) {
            sTelegram.buffer[++lge] = 0;
            sTelegram.buffer[++lge] = 0;
        }
        break;
    }

    for (int i = 0; i < pzd_anz; i++) {
        if (PZD) {
            sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PZD))) >> 8;
            sTelegram.buffer[++lge] = (*(reinterpret_cast<uint32_t*>(PZD))) & 0xFF;
        } else {
            sTelegram.buffer[++lge] = 0;
            sTelegram.buffer[++lge] = 0;
        }
    }
    sTelegram.LGE = lge;
    for (int i = 0; i < sTelegram.LGE + 1; i++) {
        bcc ^= sTelegram.buffer[i];
    }
    sTelegram.BCC = bcc;

    if (DebugDecodedTelegram) {
        MessageOutput.printf("%s ADR: %02X, LGE: %02X, PKE: %02X%02X, IND: %02X%02X", TAG,
            sTelegram.ADR,
            sTelegram.LGE,
            sTelegram.PKE.H, sTelegram.PKE.L,
            sTelegram.IND.H, sTelegram.IND.L);
        if (TaskID <= RequestText) {
            MessageOutput.printf(", PWE1: %02X%02X", sTelegram.PWE1.H, sTelegram.PWE1.L);
            if (PKW_ANZ == 4) {
                MessageOutput.printf(", PWE2: %02X%02X", sTelegram.PWE2.H, sTelegram.PWE2.L);
            }

        } else {
            MessageOutput.print(", PWE: [");
            for (int i = 0; i < 16; i++) {
                MessageOutput.printf("%c", sTelegram.buffer[7 + i]);
            }
            MessageOutput.print("]");
        }

        if (pzd_anz > 0) {
            uint8_t idx = TaskID > RequestText ? 7 + 16 : (PKW_ANZ == 4 ? 11 : 9);
            MessageOutput.printf(", PZD%d: [", PZD_ANZ);
            for (int i = 0; i < pzd_anz; i++) {
                if (i > 0)
                    MessageOutput.write(' ');
                MessageOutput.printf("%02X%02X", sTelegram.buffer[idx + i * 2 + 1], sTelegram.buffer[idx + i * 2]);
            }
            MessageOutput.write(']');
        }
        MessageOutput.printf(", BCC: %02X\r\n", sTelegram.BCC);
    }
}

void REFUsolRS485ReceiverClass::sendTelegram(void)
{
    //  int StartInterval = (int)((1000000.0*2*(1+8+1+1))/RS485BaudRate+0.5);
    //  digitalWrite(RTSPin, RS485Transmit);
    //  delayMicroseconds(StartInterval);
    REFUsolSerial.write(sTelegram.buffer, sTelegram.LGE + 1);
    REFUsolSerial.write(sTelegram.BCC);
    REFUsolSerial.flush();
    //  digitalWrite(RTSPin, RS485Receive);
}

uint8_t REFUsolRS485ReceiverClass::receiveTelegram(void)
{
    uint16_t idx = 0;
    uint32_t TimeOut;
    bool error = false;

    uint32_t t1 = millis(); // maximum response time 20ms timeout for first character
    while (1) {
        if (REFUsolSerial.available())
            break;
        if (millis() - t1 > MaximumResponseTime)
            return idx;
    }
    idx++;
    rTelegram.STX = REFUsolSerial.read();

    t1 = millis(); // 10ms timeout for third character
    while (1) {
        if (REFUsolSerial.available())
            break;
        if (millis() - t1 > 10)
            return idx;
    }
    idx++;
    rTelegram.LGE = REFUsolSerial.read();
    if (rTelegram.LGE < 3) {
        MessageOutput.printf("%s ERROR: receive buffer, LGE too small %d\r\n", TAG, rTelegram.LGE);
        error = true;
        goto exitReceive;
    }
    if (rTelegram.LGE > 254) {
        MessageOutput.printf("%s ERROR: receive buffer, LGE too large %d\r\n", TAG, rTelegram.LGE);
        error = true;
        goto exitReceive;
    }

    t1 = millis(); // 10ms timeout for second character
    while (1) {
        if (REFUsolSerial.available())
            break;
        if (millis() - t1 > 10)
            return idx;
    }
    idx++;
    rTelegram.ADR = REFUsolSerial.read();

    TimeOut = (uint32_t)((1 + 8 + 1 + 1) * 1000.0 / RS485BaudRate * rTelegram.LGE) + 20;
    t1 = millis(); // 0.625ms * LGE timeout for remaining character
    while (1) {
        if (REFUsolSerial.available())
            rTelegram.buffer[idx++] = REFUsolSerial.read();
        if (millis() - t1 > TimeOut)
            break;
        if (idx > 255) {
            MessageOutput.printf("%s ERROR: framesize, receive buffer overrun\r\n", TAG);
            error = true;
            break;
        }
        if (error) {
        }
    }

exitReceive:;
    debugRawTelegram(&rTelegram, idx);

    return idx;
}

void REFUsolRS485ReceiverClass::DebugNoResponse(uint16_t p)
{
    MessageOutput.printf("%s D%d: timeout, no response from REFUSOL\n", TAG, p);
}

void REFUsolRS485ReceiverClass::parse(void)
{
    if (!receiveTelegram()) {
        // DebugNoResponse(p);
        return;
    }

    uint16_t PNU = 0;
    PNU = (rTelegram.PKE.H & 0xF) << 8;
    PNU |= rTelegram.PKE.L;

    switch (PNU) {
    case 1104:
        Frame.dcVoltage = PWE2Float(1.0);
        MessageOutput.printf("%s DC Voltage: %.1f V\r\n", TAG, Frame.dcVoltage);
        break;
    case 1105:
        Frame.dcCurrent = PWE2Float(0.1);
        MessageOutput.printf("%s DC Current: %.1f V\r\n", TAG, Frame.dcCurrent);
        break;
    case 1106:
        Frame.acPower = PWE2Float(1.0);
        MessageOutput.printf("%s AC Power: %.0f W\r\n", TAG, Frame.acPower);
        break;
    case 1107:
        Frame.dcPower = PWE2Float(1.0);
        MessageOutput.printf("%s DC Power: %.0f W\r\n", TAG, Frame.dcPower);
        break;
    case 1123:
        Frame.acVoltage = PWE2Float(0.1);
        MessageOutput.printf("%s L1-L3 (eff): %.1f V\r\n", TAG, Frame.acVoltage);
        break;
    case 1121:
        switch (rTelegram.IND.W & 0x0F) {
        case 0:
            Frame.acVoltageL1 = PWE2Float(0.1);
            MessageOutput.printf("%s L1         : %.1f V\r\n", TAG, Frame.acVoltageL1);
            break;
        case 1:
            Frame.acVoltageL2 = PWE2Float(0.1);
            MessageOutput.printf("%s L2         : %.1f V\r\n", TAG, Frame.acVoltageL2);
            break;
        case 2:
            Frame.acVoltageL3 = PWE2Float(0.1);
            MessageOutput.printf("%s L3         : %.1f V\r\n", TAG, Frame.acVoltageL3);
            break;
        }
        break;
    case 1124:
        Frame.acCurrent = PWE2Float(0.1);
        MessageOutput.printf("%s Summe L1-L3: %.1f A\r\n", TAG, Frame.acCurrent);
        break;
    case 1141:
        switch (rTelegram.IND.W & 0x0F) {
        case 0:
            Frame.acCurrentL1 = PWE2Float(0.1);
            MessageOutput.printf("%s L1         : %.1f A\r\n", TAG, Frame.acCurrentL1);
            break;
        case 1:
            Frame.acCurrentL2 = PWE2Float(0.1);
            MessageOutput.printf("%s L2         : %.1f A\r\n", TAG, Frame.acCurrentL2);
            break;
        case 2:
            Frame.acCurrentL3 = PWE2Float(0.1);
            MessageOutput.printf("%s L3         : %.1f A\r\n", TAG, Frame.acCurrentL3);
            break;
        }
        break;
    case 1122:
        switch (rTelegram.IND.W & 0x0F) {
        case 0:
            Frame.freqL1 = PWE2Float(0.1);
            MessageOutput.printf("%s L1 : %.1f Hz\r\n", TAG, Frame.freqL1);
            break;
        case 1:
            Frame.freqL2 = PWE2Float(0.1);
            MessageOutput.printf("%s L2 : %.1f Hz\r\n", TAG, Frame.freqL2);
            break;
        case 2:
            Frame.freqL3 = PWE2Float(0.1);
            MessageOutput.printf("%s L3 : %.1f Hz\r\n", TAG, Frame.freqL3);
            break;
        }
        break;
    case 1150:
        Frame.YieldDay = PWE2Float(0.001);
        MessageOutput.printf("%s Today       : %.1f W\r\n", TAG, Frame.YieldDay);
        break;
    case 1151:
        Frame.YieldTotal = PWE2Float(0.001);
        MessageOutput.printf("%s Total       : %.3f kWh\r\n", TAG, Frame.YieldTotal);
        break;
    case 1152:
        Frame.totalOperatingHours = PWE2Uint32();
        MessageOutput.printf("%s Gesamtbetriebsstunden: %u\r\n", TAG, Frame.totalOperatingHours);
        break;
    case 1153:
        Frame.YieldMonth = PWE2Float(0.001);
        MessageOutput.printf("%s Actual month: %.3f kWh\r\n", TAG, Frame.YieldMonth);
        break;
    case 1154:
        Frame.YieldYear = PWE2Float(0.001);
        MessageOutput.printf("%s Actual year : %.3f kWh\r\n", TAG, Frame.YieldYear);
        break;
    case 1155:
        Frame.pvPeak = PWE2Float(0.001);
        MessageOutput.printf("%s PV Nennleistung: %.3f kW\r\n", TAG, Frame.pvPeak);
        break;
    case 1162:
        Frame.pvLimit = PWE2Float(0.001);
        MessageOutput.printf("%s PV Leistungsbegrenzung: %.1f%%\r\n", TAG, Frame.pvLimit);
        break;
    case 51:
        Frame.deviceSpecificOffset = PWE2Float(0.01);
        MessageOutput.printf("%s Gerätespezifischer Offset zum Winkelversatz : %.2f°\r\n", TAG, Frame.deviceSpecificOffset);
        break;
    case 1164:
        Frame.optionCosPhi = PWE2Uint32();
        MessageOutput.printf("%s Cos Phi : %.2f°\r\n", TAG, Frame.cosPhi);
        break;
    case 1165:
        Frame.plantSpecificOffset = PWE2Float(0.01);
        MessageOutput.printf("%s Anlagenspezifischer Offset zum Winkelversatz : %.2f°\r\n", TAG, Frame.plantSpecificOffset);
        break;
    case 1166:
        Frame.fixedOffset = PWE2Float(0.01);
        MessageOutput.printf("%s Fixer Offset zum Winkelversatz : %.2f°\r\n", TAG, Frame.fixedOffset);
        break;
    case 1167:
        Frame.variableOffset = PWE2Float(0.01);
        MessageOutput.printf("%s Variabler Offset zum Winkelversatz : %.2f°\r\n", TAG, Frame.variableOffset);
        break;
    case 1169:
        Frame.cosPhi = PWE2Float(0.01);
        break;
    case 1193:
        Frame.temperatureExtern = PWE2Float(0.1);
        MessageOutput.printf("%s Temperatur extern Sensor %.1f°C\r\n", TAG, Frame.temperatureExtern);
        break;
    case 92:
        switch (rTelegram.IND.W & 0x0F) {
        case 0:
            Frame.temperatureRight = PWE2Float(0.1);
            MessageOutput.printf("%s Kühlkörper rechts %.1f°C\r\n", TAG, Frame.temperatureRight);
            break;
        case 1:
            Frame.temperatureTopLeft = PWE2Float(0.1);
            MessageOutput.printf("%s Temperatur Gerät innen oben links: %.1f°C\r\n", TAG, Frame.temperatureTopLeft);
            break;
        case 2:
            Frame.temperatureBottomRight = PWE2Float(0.1);
            MessageOutput.printf("%s Temperatur Gerät innen unten rechts: %.1f°C\r\n", TAG, Frame.temperatureBottomRight);
            break;
        case 3:
            Frame.temperatureLeft = PWE2Float(0.1);
            MessageOutput.printf("%s Temperatur Kühlkörper links: %.1f°C\r\n", TAG, Frame.temperatureLeft);
            break;
        }
        break;
    case 500:
        MessageOutput.printf("%s Device Error: %d\r\n", TAG, PWE2Uint32());
        break;
    case 501:
        MessageOutput.printf("%s Actual Status: %d\r\n", TAG, PWE2Uint32());
        break;
    case 2000:
        MessageOutput.printf("%s actual Password: %d\r\n", TAG, PWE2Uint32());
        break;
    default:;
    }
}

void REFUsolRS485ReceiverClass::getDCvoltage(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1104, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getDCcurrent(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1105, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getACpower(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1106, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getDCpower(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1107, 0);
    sendTelegram();
}

/*
 * ind  0 : Voltage effective
 * ind  1 : Voltage peak of L1
 * ind  2 : Voltage peak of L2
 * ind  3 : Voltage peak of L3
 */
void REFUsolRS485ReceiverClass::getACvoltage(uint8_t adr, uint16_t ind)
{
    if (ind == 0) {
        computeTelegram(adr, false, RequestPWE, 1123, 0);
    } else {
        if (ind > 3)
            ind = 3;
        computeTelegram(adr, false, RequestPWE, 1121, ind - 1);
    }
    sendTelegram();
}

/*
 * ind  0 : Current Sum of L1, L2, L3
 * ind  1 : Current of L1
 * ind  2 : Current of L2
 * ind  3 : Current of L3
 */
void REFUsolRS485ReceiverClass::getACcurrent(uint8_t adr, uint16_t ind)
{
    if (ind == 0) {
        computeTelegram(adr, false, RequestPWE, 1124, 0);
    } else {
        if (ind > 3)
            ind = 3;
        computeTelegram(adr, false, RequestPWE, 1141, ind - 1);
    }
    sendTelegram();
}

/*
 * ind  1 : Frequency of L1
 * ind  2 : Frequency of L2
 * ind  3 : Frequency of L3
 */
void REFUsolRS485ReceiverClass::getACfrequency(uint8_t adr, uint16_t ind)
{
    if (ind < 1)
        ind = 1;
    if (ind > 3)
        ind = 3;
    computeTelegram(adr, false, RequestPWE, 1122, ind - 1);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getYieldDay(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1150, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getTotalOperatingHours(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1152, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getYieldMonth(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1153, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getYieldYear(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1154, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getYieldTotal(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1151, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getPVpeak(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1155, 0);
    sendTelegram();
}

// get power limit value in 0.1%
void REFUsolRS485ReceiverClass::getPVlimit(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1162, 0);
    sendTelegram();
}

// get Device specific angle Cos Phi in 0.01°, defult is 0.07°
void REFUsolRS485ReceiverClass::getDeviceSpecificOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 51, 0);
    sendTelegram();
}

// get Plant specific angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getPlantSpecificOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1165, 0);
    sendTelegram();
}

// get fixed angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getFixedOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1166, 0);
    sendTelegram();
}

// get variable angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getVariableOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1167, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getOptionCosPhi(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1164, 0);
    sendTelegram();
}

// get Cos Phi in 0.01°
void REFUsolRS485ReceiverClass::getCosPhi(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1169, 0);
    sendTelegram();
}

/*
 * ind  0 : external Temp Sensor
 * ind  1 : Temp Sensor left
 * ind  2 : Temp Sensor upper left
 * ind  3 : Temp Sensor bootom right
 * ind  4 : Temp Sensor right
 */
void REFUsolRS485ReceiverClass::getTemperatursensor(uint8_t adr, uint16_t ind)
{
    if (ind == 0) {
        computeTelegram(adr, false, RequestPWE, 1193, 0);
    } else {
        if (ind > 4)
            ind = 4;
        computeTelegram(adr, false, RequestPWE, 92, ind);
    }
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getFehlercode(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWEarray, 500, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getActualStatus(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWEarray, 501, 0);
    sendTelegram();
}

void REFUsolRS485ReceiverClass::getPassword(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 2000, 0);
    sendTelegram();
}

bool REFUsolRS485ReceiverClass::isDataValid()
{
    if ((millis() - getLastUpdate()) / 1000 > Configuration.get().REFUsol.PollInterval * 5) {
        return false;
    }
    return true;
}

uint32_t REFUsolRS485ReceiverClass::getLastUpdate()
{
    return _lastPoll;
}

/*
 * setLastUpdate
 * This function is called every time a new ve.direct frame was read.
 */
void REFUsolRS485ReceiverClass::setLastUpdate()
{
    _lastPoll = millis();
}

template <typename T>
void addDeviceValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["deviceValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addAcValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["acValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addYieldValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["yieldValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

template <typename T>
void addDcValue(JsonVariant& root, std::string const& name,
    T&& value, std::string const& unit, uint8_t precision)
{
    auto jsonValue = root["dcValues"][name];
    jsonValue["v"] = value;
    jsonValue["u"] = unit;
    jsonValue["d"] = precision;
}

void REFUsolRS485ReceiverClass::generateJsonResponse(JsonVariant& root)
{
    // device info
    root["data_age"] = (millis() - getLastUpdate()) / 1000;
    root["age_critical"] = !isDataValid();
    root["PID"] = "REFUsol 008K";
    root["serNo"] = Frame.serNo;
    root["firmware"] = Frame.firmware;

    addDeviceValue(root, "currentStateOfOperation", Frame.currentStateOfOperation, "", 0);
    addDeviceValue(root, "error", Frame.error, "", 0);
    addDeviceValue(root, "status", Frame.status, "", 0);
    addDeviceValue(root, "totalOperatingHours", Frame.totalOperatingHours, "Stunden", 0);
    addDeviceValue(root, "temperatureExtern", Frame.temperatureExtern, "°C", 1);
    addDeviceValue(root, "temperatureRight", Frame.temperatureRight, "°C", 1);
    addDeviceValue(root, "temperatureTopLeft", Frame.temperatureTopLeft, "°C", 1);
    addDeviceValue(root, "temperatureBottomRight", Frame.temperatureBottomRight, "°C", 1);
    addDeviceValue(root, "temperatureLeft", Frame.temperatureLeft, "°C", 1);

    // AC info
    addAcValue(root, "acPower", Frame.acPower, "kW", 3);
    addAcValue(root, "acCurrent", Frame.acCurrent, "A", 1);
    addAcValue(root, "acCurrentL1", Frame.acCurrentL1, "A", 1);
    addAcValue(root, "acCurrentL2", Frame.acCurrentL2, "A", 1);
    addAcValue(root, "acCurrentL3", Frame.acCurrentL3, "A", 1);
    addAcValue(root, "acVoltage", Frame.acVoltage, "V", 1);
    addAcValue(root, "acVoltageL1", Frame.acVoltageL1, "V", 1);
    addAcValue(root, "acVoltageL2", Frame.acVoltageL2, "V", 1);
    addAcValue(root, "acVoltageL3", Frame.acVoltageL3, "V", 1);
    addAcValue(root, "freqL1", Frame.freqL1, "Hz", 1);
    addAcValue(root, "freqL2", Frame.freqL2, "Hz", 1);
    addAcValue(root, "freqL3", Frame.freqL3, "Hz", 1);
    addAcValue(root, "cosPhi", Frame.cosPhi, "°", 1);

    // panel info
    addDcValue(root, "dcPower", Frame.dcPower, "kW", 3);
    addDcValue(root, "dcVoltage", Frame.dcVoltage, "V", 1);
    addDcValue(root, "dcCurrent", Frame.dcCurrent, "A", 1);

    addYieldValue(root, "YieldDay", Frame.YieldDay, "kWh", 3);
    addYieldValue(root, "YieldMonth", Frame.YieldMonth, "kWh", 3);
    addYieldValue(root, "YieldYear", Frame.YieldYear, "kWh", 3);
    addYieldValue(root, "YieldTotal", Frame.YieldTotal, "kWh", 3);
    addYieldValue(root, "pvPeak", Frame.pvPeak, "kW", 3);
    addYieldValue(root, "pvLimit", Frame.pvLimit, "kW", 3);

    /*
        String output;
        serializeJson(root, output);
        MessageOutput.println(output);
    */
}

#endif
