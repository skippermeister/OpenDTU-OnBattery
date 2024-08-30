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
#include "SerialPortManager.h"
#include <Arduino.h>
#include <driver/uart.h>

REFUsolRS485ReceiverClass REFUsol;

REFUsolRS485ReceiverClass::REFUsolRS485ReceiverClass()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&REFUsolRS485ReceiverClass::loop, this))
    , _lastPoll(0)
{
}

void REFUsolRS485ReceiverClass::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize REFUsol Inverter... ");

    scheduler.addTask(_loopTask);

    updateSettings();

    MessageOutput.println("done");
}

void REFUsolRS485ReceiverClass::updateSettings(void)
{
    if (_initialized) {
        deinit();
        _loopTask.disable();
    }

    auto const& cREFUsol = Configuration.get().REFUsol;
    if (!cREFUsol.Enabled) {
        MessageOutput.println("REFUsol receiver not enabled");
        return;
    }

    _verboseLogging = cREFUsol.VerboseLogging;

    auto const& pin = PinMapping.get().REFUsol;
    if (!PinMapping.isValidREFUsolConfig()) {
        MessageOutput.printf("Invalid TX=%d/RX=%d/RTS=%d pin config", pin.rx, pin.tx, pin.rts);
        return;
    }

    adr = cREFUsol.USSaddress;

    if (!_initialized) {
        auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
        if (!oHwSerialPort) { return; }

        _upSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);

        _upSerial->begin(cREFUsol.Baudrate,
            cREFUsol.Parity==REFUsol_CONFIG_T::Parity_t::Even?SERIAL_8E1:
            cREFUsol.Parity==REFUsol_CONFIG_T::Parity_t::Odd?SERIAL_8O1:
            SERIAL_8N1,
            pin.rx, pin.tx);
        static constexpr const char *Parity[3] = {"NONE", "EVEN", "ODD"};
        MessageOutput.printf("RS485 (Type %d) @ %u Baud with %s parity, USS adr = %u, port rx = %d, tx = %d",
            pin.rts >= 0 ? 1 : 2,
            cREFUsol.Baudrate, Parity[cREFUsol.Parity],
            cREFUsol.USSaddress,
            pin.rx, pin.tx);
        _StartInterval = static_cast<int>((1000000.0*2*(1+8+1+1))/cREFUsol.Baudrate+0.5);
        RS485BaudRate = cREFUsol.Baudrate;
        if (pin.rts >= 0) {
            /*
             * REFUsol inverter is connected via a RS485 module. Two different types of modules are supported.
             * Type 1: if a GPIO pin greater or equal 0 is given, we have a MAX3485 or SP3485 modul with external driven DE/RE pins
             *         Both pins are connected together and will be driven by the HWSerial driver.
             * Type 2: if the GPIO is negativ (-1), we assume that we have a RS485 TTL Modul with a self controlled DE/RE circuit.
             *         In this case we only need a TX and RX pin.
             */
            MessageOutput.printf(", rts = %d", pin.rts);
            _upSerial->setPins(pin.rx, pin.tx, UART_PIN_NO_CHANGE, pin.rts);
        }
        ESP_ERROR_CHECK(uart_set_mode(*oHwSerialPort, UART_MODE_RS485_HALF_DUPLEX));

        // Set read timeout of UART TOUT feature
        ESP_ERROR_CHECK(uart_set_rx_timeout(*oHwSerialPort, ECHO_READ_TOUT));

        _upSerial->flush();

        while (_upSerial->available()) { // clear RS485 read buffer
            _upSerial->read();
            vTaskDelay(1);
        }

        _initialized = true;
        _allParametersRead = false;

        _loopTask.enable();

        MessageOutput.println(" initialized successfully.");
    }
}

bool REFUsolRS485ReceiverClass::readParameters()
{
    static int8_t state=0;
    static int8_t index=1;

    if (_allParametersRead) { return false; }

    if (_verboseLogging) MessageOutput.printf("%s Read basic infos and parameters ... ", TAG);

    switch(state) {
        case 0:
            if (_verboseLogging) MessageOutput.println("get total operating hours");
            getTotalOperatingHours(adr);
            state++;
            return true;

        case 1:
            if (_verboseLogging) MessageOutput.println("get PV peak");
            getPVpeak(adr);
            state++;
            return true;

        case 2:
            if (_verboseLogging) MessageOutput.println("get PV limit");
            getPVlimit(adr);
            state++;
            return true;

        case 3:
            if (_verboseLogging) MessageOutput.println("get device specific offset");
            getDeviceSpecificOffset(adr);
            state++;
            return true;

        case 4:
            if (_verboseLogging) MessageOutput.println("get plant specific offset");
            getPlantSpecificOffset(adr);
            state++;
            return true;

        case 5:
            if (_verboseLogging) MessageOutput.println("get option cos phi");
            getOptionCosPhi(adr);
            state++;
            return true;

        case 6:
            switch (Frame.optionCosPhi) {
                case 0:
                    break;
                case 1:
                    if (_verboseLogging) MessageOutput.println("get fixed offset");
                    getFixedOffset(adr);
                    state++;
                    return true;
                case 2:
                    if (_verboseLogging) MessageOutput.println("get variable offset");
                    getVariableOffset(adr);
                    state++;
                    return true;
                case 3:
                    if (_verboseLogging) MessageOutput.printf("get cos phi(P=%d%%)\r\n", (index-1)*10);
                    getCosPhi(adr, index++);
                    if (index>11) state++;
                    return true;
                case 4:
                    if (_verboseLogging) MessageOutput.println("get cos phi");
                    getCosPhi(adr, 0);
                    state++;
                    return true;
                default:;
            }
            break;

        default:;
    }

    Frame.effectivCosPhi = NaN;
    switch (Frame.optionCosPhi) {
    case 0:
        Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset;
        break;
    case 1:
        if (Frame.fixedOffset > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.fixedOffset;
        break;
    case 2:
        if (Frame.variableOffset > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.variableOffset;
        break;
    case 3:
        if (Frame.cosPhiPvPeak[0] > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.cosPhiPvPeak[0];
        break;
    case 4:
        if (Frame.cosPhi > NaN)
            Frame.effectivCosPhi = Frame.deviceSpecificOffset + Frame.plantSpecificOffset + Frame.cosPhi;
        break;
    default:;
        MessageOutput.printf("%s unknown option cos phi: %d\r\n", TAG, Frame.optionCosPhi);
    }

    if (_verboseLogging) {
        if (Frame.optionCosPhi > NaN) MessageOutput.printf("Effektive Cos Phi : %.2f°\r\n", Frame.effectivCosPhi);
    }

    state=0;
    index=1;
    _allParametersRead = true;

    return true;
}

void REFUsolRS485ReceiverClass::deinit(void)
{
    if (!_initialized) {
        return;
    }

    _upSerial->end();

    if (PinMapping.get().REFUsol.rts >= 0) { pinMode(PinMapping.get().REFUsol.rts, INPUT); }

    SerialPortManager.freePort(_serialPortOwner);

    _initialized = false;

    MessageOutput.printf("%s RS485 driver uninstalled\r\n", TAG);
}

void REFUsolRS485ReceiverClass::loop()
{
    if (!Configuration.get().REFUsol.Enabled) {
        deinit();
        return;
    }

    if (!_initialized) {
        updateSettings();
        return;
    }

    uint32_t t_start = millis();

    if (_upSerial->available()) {
        parse();
    }

    if ((millis() - getLastUpdate()) < Configuration.get().REFUsol.PollInterval * 1000) {
        return;
    }

    if (readParameters()) {
        setLastUpdate();
        return;
    }

    static int state = 0;
    static int state2 = 0;
    switch (state) {
    case 0:
    case 5:
        if (_verboseLogging) MessageOutput.printf("%s get ", TAG);
        switch (state2) {
            case 0:
                if (_verboseLogging) MessageOutput.println("DC power");
                getDCpower(adr);
                state2++;
                break;
            case 1:
                if (_verboseLogging)MessageOutput.println("DC current");
                getDCcurrent(adr);
                state2++;
                break;
            case 2:
                if (_verboseLogging)MessageOutput.println("DC voltage");
                getDCvoltage(adr);
                state2++;
                break;
            case 3:
                if (_verboseLogging)MessageOutput.println("AC power");
                getACpower(adr);
                state2=0;
                state++;
        }
        break;

    case 1:
    case 6:
        if (_verboseLogging) MessageOutput.printf("%s get AC voltage index: %d\r\n", TAG, state2);
        switch (state2) {
            case 0:
                getACvoltage(adr, 0);
                state2++;
                break;
            case 1:
                getACvoltage(adr, 1);
                state2++;
                break;
            case 2:
                getACvoltage(adr, 2);
                state2++;
                break;
            case 3:
                getACvoltage(adr, 3);
                state2=0;
                state++;
        }
        break;

    case 2:
    case 7:
        if (_verboseLogging) MessageOutput.printf("%s get AC current index: %d\r\n", TAG, state2);
        switch (state2) {
            case 0:
                getACcurrent(adr, 0);
                state2++;
                break;
            case 1:
                getACcurrent(adr, 1);
                state2++;
                break;
            case 2:
                getACcurrent(adr, 2);
                state2++;
                break;
            case 3:
                getACcurrent(adr, 3);
                state2=0;
                state++;
        }
        break;

    case 3:
    case 8:
        if (_verboseLogging) MessageOutput.printf("%s get phase frequency index: %d\r\n", TAG, state2);
        switch (state2) {
            case 0:
                getACfrequency(adr, 1);
                state2++;
                break;
            case 1:
                getACfrequency(adr, 2);
                state2++;
                break;
            case 2:
                getACfrequency(adr, 3);
                state2=0;
                state++;
        }
        break;

    case 4:
    case 9:
        if (_verboseLogging) MessageOutput.printf("%s get yield solar power ", TAG);
        switch (state2) {
            case 0:
                if (_verboseLogging) MessageOutput.println("today");
                getYieldDay(adr);
                state2++;
                break;
            case 1:
                if (_verboseLogging) MessageOutput.println("month");
                getYieldMonth(adr);
                state2++;
                break;
            case 2:
                if (_verboseLogging) MessageOutput.println("year");
                getYieldYear(adr);
                state2++;
                break;
            case 3:
                if (_verboseLogging) MessageOutput.println("total");
                getYieldTotal(adr);
                state2=0;
                state++;
        }
        break;

    case 10:
        if (_verboseLogging) MessageOutput.printf("%s get temperature sensor index: %d\r\n", TAG, state2);
        switch (state2) {
            case 0:
                getTemperatursensor(adr, 1);
                state2++;
                break;
            case 1:
                getTemperatursensor(adr, 2);
                state2++;
                break;
            case 2:
                getTemperatursensor(adr, 3);
                state2++;
                break;
            case 3:
                getTemperatursensor(adr, 4);
                state2=0;
                state++;
        }
        break;
    case 11:
        if (_verboseLogging) MessageOutput.printf("%s get error code\r\n", TAG);
        getFehlercode(adr);
        state++;
        break;
    case 12:
        if (_verboseLogging) MessageOutput.printf("%s get actual status\r\n", TAG);
        getActualStatus(adr);
        state++;
        break;
    case 13:
        if (_verboseLogging) MessageOutput.printf("%s get total operating hours\r\n", TAG);
        getTotalOperatingHours(adr);
        state=0;
    }

    setLastUpdate();

    MessageOutput.printf("%s Round trip %lu ms\r\n", TAG, millis() - t_start);
}

void REFUsolRS485ReceiverClass::debugRawTelegram(TELEGRAM_t* t, uint8_t len)
{
    if (!_verboseLogging) return;

    if (DebugRawTelegram) {
        if (t->STX == 0x02) {   // valid frame received
            MessageOutput.printf("STX: 0X%02X LGE: %u, ADR: %d, ", t->STX, t->LGE, t->ADR);
            uint16_t PNU = ((rTelegram.PKE.H & 0xF) << 8) | rTelegram.PKE.L;
            uint8_t TaskID = (t->PKE.H)>>4;
            uint8_t SP = ((t->PKE.H & 0x08) >> 3);
            MessageOutput.printf("PKE: 0X%02X%02X, TaskID: %1X, SP: %u, PNU: %u, ", t->PKE.H, t->PKE.L, TaskID, SP, PNU);
            MessageOutput.printf("IND: 0X%02X%02X, ", t->IND.H, t->IND.L);
            MessageOutput.printf("PWE:[0X%02X%02X, ", t->PWE1.H, t->PWE1.L);
            MessageOutput.printf("0X%02X%02X] \r\n", t->PWE2.H, t->PWE2.L);

            for (uint8_t i = 11; i < len; i++) {
                MessageOutput.printf("%02X ", t->buffer[i]);
            }
            MessageOutput.println();

        } else {
            for (uint8_t i = 0; i < len; i++) {
                MessageOutput.printf("%02X ", t->buffer[i]);
            }
            MessageOutput.println();
        }
    }
}

float REFUsolRS485ReceiverClass::PWE2Float(float scale)
{
    LittleEndianDoubleWord_t value;

    value.HH = rTelegram.PWE1.H;
    value.HL = rTelegram.PWE1.L;
    value.LH = rTelegram.PWE2.H;
    value.LL = rTelegram.PWE2.L;
    return static_cast<float>(value.DW) * scale;
}

uint32_t REFUsolRS485ReceiverClass::PWE2Uint32(void)
{
    LittleEndianDoubleWord_t value;

    value.HH = rTelegram.PWE1.H;
    value.HL = rTelegram.PWE1.L;
    value.LH = rTelegram.PWE2.H;
    value.LL = rTelegram.PWE2.L;
    return value.DW;
}

void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND)
{
    compute(adr, mirror, TaskID, 0, PNU, IND, reinterpret_cast<void*>(NULL), PZD_ANZ,  reinterpret_cast<void*>(NULL));
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE)
{
    compute(adr, mirror, TaskID, 0, PNU, IND, PWE, PZD_ANZ, reinterpret_cast<void*>(NULL));
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, void* PZD)
{
    compute(adr, mirror, TaskID, 0, PNU, IND, PWE, PZD_ANZ, PZD);
}
void REFUsolRS485ReceiverClass::computeTelegram(uint8_t adr, bool mirror, uint16_t TaskID, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD)
{
    compute(adr, mirror, TaskID, 0, PNU, IND, PWE, pzd_anz, PZD);
}

void REFUsolRS485ReceiverClass::compute(uint8_t adr, bool mirror, uint16_t TaskID, uint8_t SP, uint16_t PNU, int16_t IND, void* PWE, uint8_t pzd_anz, void* PZD)
{
    uint8_t bcc = 0;
    uint8_t lge = 0;

    adr &= 0b00011111;
    sTelegram.STX = 0x02;
    sTelegram.ADR = adr | (mirror ? 0b01000000 : 0b00000000);
    lge += 2;

    sTelegram.PKE.H = ((PNU & 0b0000011111111111) | (SP << 11) | ((TaskID & 0b1111) << 12)) >> 8;
    sTelegram.PKE.L =  (PNU & 0b0000011111111111) & 0xFF;
    lge += 2;

    sTelegram.IND.H = (IND | (TaskID & 0b1100000000)) >> 8;
    sTelegram.IND.L = IND & 0xFF;
    lge += 2;

    switch (TaskID) {
    case 0b0000:    // No task
    case 0b0001:    // Request PWE
    case 0b0010:    // Change PWE (word)
    case 0b0110:    // Request PWE (array) 1)
    case 0b0111:    // Change PWE (array word) 1)
    case 0b1001:    // Request the number of array elements
    case 0b1100:    // Change PWE (array word), and store in the EEPROM 1), 2)
    case 0b1110:    // Change PWE (word), and store in the EEPROM 2)
        if (PKW_ANZ > 3) {
            sTelegram.buffer[++lge] = 0;    // PWE1.H
            sTelegram.buffer[++lge] = 0;    // PWE1.L
        }
        if ((TaskID == 0b0000) || (TaskID == 0b0001) || (TaskID == 0b0110) || (TaskID == 0b1001)) {
            sTelegram.buffer[++lge] = 0;    // PWE2.H
            sTelegram.buffer[++lge] = 0;    // PWE2.L
        } else {
            sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianWord_t*>(PWE))).H;   // PWE2.H
            sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianWord_t*>(PWE))).L; // PWE2.L
        }
        break;

    case 0b0011:    // Change PWE (double word)
    case 0b1000:    // ChangePWEarray
    case 0b1101:    // Change PWE (double word), and store in EEPROM 2)
        sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianDoubleWord_t*>(PWE))).HH;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianDoubleWord_t*>(PWE))).HL;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianDoubleWord_t*>(PWE))).LH;
        sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianDoubleWord_t*>(PWE))).LL;
        break;

    case 0b1000001111:   // Change first text field
    case 0b1100001111:   // Change second text field
        {
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
    case 0b0000001111:   // Request first text field
    case 0b0100001111:   // Request second text field
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
            sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianWord_t*>(PZD))).H;
            sTelegram.buffer[++lge] = (*(reinterpret_cast<LittleEndianWord_t*>(PZD))).L;
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

    if (_verboseLogging && DebugDecodedTelegram) {
        MessageOutput.printf("%s ADR: %u, LGE: %u, TaskID: %1X, SP: %u, PNU: %u, PKE: 0x%02X%02X, IND: 0x%02X%02X", TAG,
            sTelegram.ADR,
            sTelegram.LGE,
            (sTelegram.PKE.H>>4) & 0xF, // TaskID
            (sTelegram.PKE.H & 0x08)>>3,    // SP bit
            PNU,
            sTelegram.PKE.H, sTelegram.PKE.L,
            sTelegram.IND.H, sTelegram.IND.L);
        if (TaskID <= RequestText) {
            MessageOutput.printf(", PWE:[0x%02X%02X", sTelegram.PWE1.H, sTelegram.PWE1.L);
            if (PKW_ANZ == 4) {
                MessageOutput.printf(" 0x%02X%02X", sTelegram.PWE2.H, sTelegram.PWE2.L);
            }
            MessageOutput.print("]");

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
                MessageOutput.printf("0x%02X%02X", sTelegram.buffer[idx + i * 2 + 1], sTelegram.buffer[idx + i * 2]);
            }
            MessageOutput.write(']');
        }
        MessageOutput.printf(", BCC: %02X\r\n", sTelegram.BCC);
    }

    sendTelegram();

//    parse();
}

void REFUsolRS485ReceiverClass::sendTelegram(void)
{
    if (_rtsPin >= 0) {
        digitalWrite(_rtsPin, HIGH);
        delayMicroseconds(_StartInterval);
    }

    _upSerial->write(sTelegram.buffer, sTelegram.LGE + 1);
    _upSerial->write(sTelegram.BCC);
    _upSerial->flush();

    if (_rtsPin >= 0) digitalWrite(_rtsPin, LOW);
}

uint8_t REFUsolRS485ReceiverClass::receiveTelegram(void)
{
    uint16_t idx = 0;
    uint32_t TimeOut;
    bool error = false;

    uint32_t t1 = millis(); // maximum response time 20ms timeout for first character
    while (1) {
        if (_upSerial->available())
            break;
        if (millis() - t1 > MaximumResponseTime)
            return idx;
    }
    idx++;
    rTelegram.STX = _upSerial->read();

    t1 = millis(); // 10ms timeout for third character
    while (1) {
        if (_upSerial->available())
            break;
        if (millis() - t1 > 10)
            return idx;
    }
    idx++;
    rTelegram.LGE = _upSerial->read();
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
        if (_upSerial->available())
            break;
        if (millis() - t1 > 10)
            return idx;
    }
    idx++;
    rTelegram.ADR = _upSerial->read();

    TimeOut = (uint32_t)((1 + 8 + 1 + 1) * 1000.0 / RS485BaudRate * rTelegram.LGE) + 20;
    t1 = millis(); // 0.625ms * LGE timeout for remaining character
    while (1) {
        if (_upSerial->available())
            rTelegram.buffer[idx++] = _upSerial->read();
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
// Aktueller Zustand
static constexpr char const *StringActualStatus[8] = {
  "Initialization",
  "Off",
  "Activation",
  "Standby",
  "Operation",
  "Shutdown",
  "Short outage",
  "Disturbance"
};

    if (!receiveTelegram()) {
        // DebugNoResponse(p);
        return;
    }

    uint16_t PNU = 0;
    PNU = (rTelegram.PKE.H & 0xF) << 8;
    PNU |= rTelegram.PKE.L;

    LittleEndianWord_t IND;
    IND.L = rTelegram.IND.L;
    IND.H = rTelegram.IND.H;

    switch (PNU) {
    case 1104:
        Frame.dcVoltage = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s DC voltage: %.1f V\r\n", TAG, Frame.dcVoltage);
        break;
    case 1105:
        Frame.dcCurrent = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s DC current: %.1f V\r\n", TAG, Frame.dcCurrent);
        break;
    case 1106:
        Frame.acPower = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s AC power: %.0f W\r\n", TAG, Frame.acPower);
        break;
    case 1107:
        Frame.dcPower = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s DC power: %.0f W\r\n", TAG, Frame.dcPower);
        break;
    case 1123:
        Frame.acVoltage = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s L1-L3 voltage (eff): %.1f V\r\n", TAG, Frame.acVoltage);
        break;
    case 1121:
        switch (IND.W) {
        case 0:
            Frame.acVoltageL1 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L1 voltage: %.1f V\r\n", TAG, Frame.acVoltageL1);
            break;
        case 1:
            Frame.acVoltageL2 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L2 voltage: %.1f V\r\n", TAG, Frame.acVoltageL2);
            break;
        case 2:
            Frame.acVoltageL3 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L3 voltage: %.1f V\r\n", TAG, Frame.acVoltageL3);
            break;
        default:;
            MessageOutput.printf("%s undefined AC voltage index: %d\r\n", TAG, IND.W);
        }
        break;
    case 1124:
        Frame.acCurrent = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s L1-L3 sum current: %.1f A\r\n", TAG, Frame.acCurrent);
        break;
    case 1141:
        switch (IND.W) {
        case 0:
            Frame.acCurrentL1 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L1 current: %.1f A\r\n", TAG, Frame.acCurrentL1);
            break;
        case 1:
            Frame.acCurrentL2 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L2 current: %.1f A\r\n", TAG, Frame.acCurrentL2);
            break;
        case 2:
            Frame.acCurrentL3 = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("%s L3 current: %.1f A\r\n", TAG, Frame.acCurrentL3);
            break;
        default:;
            if (_verboseLogging) MessageOutput.printf("%s undefined AC current index: %d\r\n", TAG, IND.W);
        }
        break;
    case 1122:
        switch (IND.W) {
        case 0:
            Frame.freqL1 = PWE2Float(0.01);
            if (_verboseLogging) MessageOutput.printf("%s L1: %.2f Hz\r\n", TAG, Frame.freqL1);
            break;
        case 1:
            Frame.freqL2 = PWE2Float(0.01);
            if (_verboseLogging) MessageOutput.printf("%s L2: %.2f Hz\r\n", TAG, Frame.freqL2);
            break;
        case 2:
            Frame.freqL3 = PWE2Float(0.01);
            if (_verboseLogging) MessageOutput.printf("%s L3: %.2f Hz\r\n", TAG, Frame.freqL3);
            break;
        default:;
            MessageOutput.printf("%s undefined frequency index: %d\r\n", TAG, IND.W);
        }
        break;
    case 1150:
        Frame.YieldDay = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s Yield today: %.1f W\r\n", TAG, Frame.YieldDay);
        break;
    case 1151:
        Frame.YieldTotal = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s Yield total: %.3f kWh\r\n", TAG, Frame.YieldTotal);
        break;
    case 1152:
        Frame.totalOperatingHours = PWE2Uint32();
        if (_verboseLogging) MessageOutput.printf("%s Total operating hours: %u:%u\r\n", TAG, Frame.totalOperatingHours/60, Frame.totalOperatingHours%60);
        break;
    case 1153:
        Frame.YieldMonth = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s Yield actual month: %.3f kWh\r\n", TAG, Frame.YieldMonth);
        break;
    case 1154:
        Frame.YieldYear = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s Yield actual year: %.3f kWh\r\n", TAG, Frame.YieldYear);
        break;
    case 1155:
        Frame.pvPeak = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s PV peak: %.3f kW\r\n", TAG, Frame.pvPeak);
        break;
    case 1162:
        Frame.pvLimit = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s PV limited: %.1f%%\r\n", TAG, Frame.pvLimit);
        break;
    case 51:
        Frame.deviceSpecificOffset = PWE2Float(0.01);
        if (_verboseLogging) MessageOutput.printf("%s Device specific offset: %.2f°\r\n", TAG, Frame.deviceSpecificOffset);
        break;
    case 1164:
        Frame.optionCosPhi = PWE2Uint32();
        if (_verboseLogging) MessageOutput.printf("%s Option Cos Phi: %d\r\n", TAG, Frame.optionCosPhi);
        break;
    case 1165:
        Frame.plantSpecificOffset = PWE2Float(0.01);
        if (_verboseLogging) MessageOutput.printf("%s Plant specfic offset: %.2f°\r\n", TAG, Frame.plantSpecificOffset);
        break;
    case 1166:
        Frame.fixedOffset = PWE2Float(0.01);
        if (_verboseLogging) MessageOutput.printf("%s Fixed offset: %.2f°\r\n", TAG, Frame.fixedOffset);
        break;
    case 1167:
        Frame.variableOffset = PWE2Float(0.01);
        if (_verboseLogging) MessageOutput.printf("%s Variable offset: %.2f°\r\n", TAG, Frame.variableOffset);
        break;
    case 1168:
        if (IND.W <= 10) {
            Frame.cosPhiPvPeak[IND.W] = PWE2Float(0.01);
            if (_verboseLogging) MessageOutput.printf("%s PW peak Cos Phi[%d]: %.2f°\r\n", TAG, IND.W, Frame.cosPhiPvPeak[IND.W]);
        } else {
            MessageOutput.printf("%s Cos Phi of PV peak index out of range: %d\r\n", TAG, IND.W);
        }

        break;
    case 1169:
        Frame.cosPhi = PWE2Float(0.01);
        if (_verboseLogging) MessageOutput.printf("%s Cos Phi: %.2f°\r\n", TAG, Frame.cosPhi);
        break;
    case 1193:
        Frame.temperatureExtern = PWE2Float(0.1);
        if (_verboseLogging) MessageOutput.printf("%s Temperatur extern Sensor %.1f°C\r\n", TAG, Frame.temperatureExtern);
        break;
    case 92:
        if (_verboseLogging) MessageOutput.printf("%s Temperature ", TAG);
        switch (IND.W) {
        case 0:
            Frame.temperatureRight = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("heat sink right %.1f°C\r\n", Frame.temperatureRight);
            break;
        case 1:
            Frame.temperatureTopLeft = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("device internal upper left: %.1f°C\r\n", Frame.temperatureTopLeft);
            break;
        case 2:
            Frame.temperatureBottomRight = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("device internal bottom right: %.1f°C\r\n", Frame.temperatureBottomRight);
            break;
        case 3:
            Frame.temperatureLeft = PWE2Float(0.1);
            if (_verboseLogging) MessageOutput.printf("heat sink left: %.1f°C\r\n", Frame.temperatureLeft);
            break;
        default:;
            MessageOutput.printf("\r\n%s undefined sensor index: %d\r\n", TAG, IND.W);
        }
        break;
    case 500:
        Frame.error = PWE2Uint32();
        if (_verboseLogging) MessageOutput.printf("%s Device Error: 0x%06X\r\n", TAG, Frame.error);
        break;
    case 501:
        Frame.status = PWE2Uint32();
        if (_verboseLogging) MessageOutput.printf("%s Actual Status: %d - %s\r\n", TAG,
            Frame.status,
            (Frame.status <= 7) ? StringActualStatus[Frame.status] : "unkown status");
        break;
    case 2000:
        if (_verboseLogging) MessageOutput.printf("%s actual Password: %d\r\n", TAG, PWE2Uint32());
        break;
    default:;
        MessageOutput.printf("%s undefined PNU: %d\r\n", TAG, PNU);
    }
}

void REFUsolRS485ReceiverClass::getDCvoltage(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1104, 0);
}

void REFUsolRS485ReceiverClass::getDCcurrent(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1105, 0);
}

void REFUsolRS485ReceiverClass::getACpower(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1106, 0);
}

void REFUsolRS485ReceiverClass::getDCpower(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1107, 0);
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
        if (ind > 3) ind = 3;
        computeTelegram(adr, false, RequestPWE, 1121, ind - 1);
    }
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
        if (ind > 3) ind = 3;
        computeTelegram(adr, false, RequestPWE, 1141, ind - 1);
    }
}

/*
 * ind  1 : Frequency of L1
 * ind  2 : Frequency of L2
 * ind  3 : Frequency of L3
 */
void REFUsolRS485ReceiverClass::getACfrequency(uint8_t adr, uint16_t ind)
{
    if (ind < 1) ind = 1;
    if (ind > 3) ind = 3;
    computeTelegram(adr, false, RequestPWE, 1122, ind - 1);
}

void REFUsolRS485ReceiverClass::getYieldDay(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1150, 0);
}

void REFUsolRS485ReceiverClass::getTotalOperatingHours(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1152, 0);
}

void REFUsolRS485ReceiverClass::getYieldMonth(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1153, 0);
}

void REFUsolRS485ReceiverClass::getYieldYear(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1154, 0);
}

void REFUsolRS485ReceiverClass::getYieldTotal(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1151, 0);
}

void REFUsolRS485ReceiverClass::getPVpeak(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1155, 0);
}

// get power limit value in 0.1%
void REFUsolRS485ReceiverClass::getPVlimit(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1162, 0);
}

// get Device specific angle Cos Phi in 0.01°, defult is 0.07°
void REFUsolRS485ReceiverClass::getDeviceSpecificOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 51, 0);
}

// get Plant specific angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getPlantSpecificOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1165, 0);
}

// get fixed angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getFixedOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1166, 0);
}

// get variable angle Cos Phi in 0.01°, default is 0.0°
void REFUsolRS485ReceiverClass::getVariableOffset(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1167, 0);
}

void REFUsolRS485ReceiverClass::getOptionCosPhi(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 1164, 0);
}

// get Cos Phi in 0.01°
void REFUsolRS485ReceiverClass::getCosPhi(uint8_t adr, uint16_t ind)
{
    if (ind == 0) {
        computeTelegram(adr, false, RequestPWE, 1169, 0);
    } else {
        if (ind > 11) ind = 11;
        computeTelegram(adr, false, RequestPWE, 1168, ind - 1);
    }
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
        if (ind > 4) ind = 4;
        computeTelegram(adr, false, RequestPWE, 92, ind);
    }
}

void REFUsolRS485ReceiverClass::getFehlercode(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 500, 0);
}

void REFUsolRS485ReceiverClass::getActualStatus(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 501, 0);
}

void REFUsolRS485ReceiverClass::getPassword(uint8_t adr)
{
    computeTelegram(adr, false, RequestPWE, 2000, 0);
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
    addYieldValue(root, "pvPeak", Frame.pvPeak, "kWp", 3);
    addYieldValue(root, "pvLimit", Frame.pvLimit, "kW", 3);

    /*
        String output;
        serializeJson(root, output);
        MessageOutput.println(output);
    */
}

#endif
