/* Library for reading SDM 72/120/220/230/630 Modbus Energy meters.
 *  Reading via Hardware or Software Serial library & rs232<->rs485 converter
 *  2016-2022 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.8.10 & 2.5.2 esp8266 core)
 *  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
 */
//------------------------------------------------------------------------------
#include "SDM.h"

//------------------------------------------------------------------------------
#if defined(USE_POWERMETER_SERIAL2)
SDM::SDM(HardwareSerial& serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin)
    : sdmSer(serial)
{
    this->_baud = baud;
    this->_dere_pin = dere_pin;
    this->_config = config;
    this->_rx_pin = rx_pin;
    this->_tx_pin = tx_pin;
}
#else
SDM::SDM(SoftwareSerial& serial, long baud, int dere_pin, int config, int8_t rx_pin, int8_t tx_pin)
    : sdmSer(serial)
{
    this->_baud = baud;
    this->_dere_pin = dere_pin;
    this->_config = config;
    this->_rx_pin = rx_pin;
    this->_tx_pin = tx_pin;
}
#endif

SDM::~SDM()
{
}

void SDM::begin(void)
{
#if defined(USE_POWERMETER_SERIAL2)
    sdmSer.begin(_baud, _config, _rx_pin, _tx_pin);
    if (_dere_pin >= 0) {
        /*
         * SDM is connected via a RS485 module. Two different types of modules are supported.
         * Type 1: if a GPIO pin greater or equal 0 is given, we have a MAX3485 or SP3485 modul with external driven DE/RE pins
         *         Both pins are connected together and will be driven by the HWSerial driver.
         * Type 2: if the GPIO is negativ (-1), we assume that we have a RS485 TTL Modul with a self controlled DE/RE circuit.
         *         In this case we only need a TX and RX pin.
         */
        sdmSer.setPins(_rx_pin, _tx_pin, UART_PIN_NO_CHANGE, _dere_pin);
    }
    ESP_ERROR_CHECK(uart_set_mode(2, UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(2, ECHO_READ_TOUT));
#else
    sdmSer.begin(_baud, (EspSoftwareSerial::Config)_config, _rx_pin, _tx_pin);
    if (_dere_pin >= 0) {
        /*
         * SDM is connected via a RS485 module. Two different types of modules are supported.
         * Type 1: if a GPIO pin greater or equal 0 is given, we have a MAX3485 or SP3485 modul with external driven DE/RE pins
         *         Both pins are connected together and will be driven by the HWSerial driver.
         * Type 2: if the GPIO is negativ (-1), we assume that we have a RS485 TTL Modul with a self controlled DE/RE circuit.
         *         In this case we only need a TX and RX pin.
         */
        sdmSer.setTransmitEnablePin(_dere_pin); // in SoftwareSerial it is handled within driver
    }
#endif
}

void sdm_debug(const char* s, uint8_t* frame, uint8_t len)
{
    Serial.printf("%s SDM: ", s);
    for (uint8_t i = 0; i < len; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();
}

float SDM::readVal(uint16_t reg, uint8_t node)
{
    uint32_t t_start = millis();

    uint16_t temp;
    unsigned long req_end;
    unsigned long resp_start = 0;
    uint8_t sdmarr[FRAMESIZE] = { node, SDM_B_02, 0, 0, SDM_B_05, SDM_B_06, 0, 0, 0 };

    union {
        uint8_t b[4];
        float value;
    } res;
    res.value = NAN;
    uint16_t readErr = SDM_ERR_NO_ERROR;

    sdmarr[2] = highByte(reg);
    sdmarr[3] = lowByte(reg);

    temp = calculateCRC(sdmarr, FRAMESIZE - 3); // calculate out crc only from first 6 bytes

    sdmarr[6] = lowByte(temp);
    sdmarr[7] = highByte(temp);

#if !defined(USE_POWERMETER_SERIAL2)
    sdmSer.listen(); // enable softserial rx interrupt
#endif

    flush(); // read serial if any old data is available

    //  sdm_debug("Write", sdmarr, FRAMESIZE - 1);

    sdmSer.write(sdmarr, FRAMESIZE - 1); // send 8 bytes

    sdmSer.flush(); // clear out tx buffer

    req_end = millis();

    while (sdmSer.available() < FRAMESIZE) {
        if(sdmSer.available()>0 && resp_start==0) resp_start = millis();
        if (millis() - req_end > msturnaround) {
            readErr = SDM_ERR_TIMEOUT; // err debug (4)
            break;
        }
        yield();
    }

    if (readErr == SDM_ERR_NO_ERROR) { // if no timeout...

        if (sdmSer.available() >= FRAMESIZE) {

            for (int n = 0; n < FRAMESIZE; n++) {
                sdmarr[n] = sdmSer.read();
            }

            if (sdmarr[0] == node && sdmarr[1] == SDM_B_02 && sdmarr[2] == SDM_REPLY_BYTE_COUNT) {

                if ((calculateCRC(sdmarr, FRAMESIZE - 2)) == ((sdmarr[8] << 8) | sdmarr[7])) { // calculate crc from first 7 bytes and compare with received crc (bytes 7 & 8)
                    res.b[3] = sdmarr[3];
                    res.b[2] = sdmarr[4];
                    res.b[1] = sdmarr[5];
                    res.b[0] = sdmarr[6];
                    /*
                              Serial.printf("%02d %02d %02d %02d | %02d %02d %02d %02d Res: %f, size of res %d\r\n", res.b[3], res.b[2], res.b[1], res.b[0],
                                sdmarr[6], sdmarr[5], sdmarr[4], sdmarr[3],
                                res.value,
                                sizeof(res));
                    */
                } else {
                    readErr = SDM_ERR_CRC_ERROR; // err debug (1)
                }

            } else {
                readErr = SDM_ERR_WRONG_BYTES; // err debug (2)
            }

        } else {
            readErr = SDM_ERR_NOT_ENOUGHT_BYTES; // err debug (3)
        }

        //    sdm_debug("Read", sdmarr, FRAMESIZE);
    }

    flush(mstimeout); // read serial if any old data is available and wait for RESPONSE_TIMEOUT (in ms)

    if (sdmSer.available()) // if serial rx buffer (after RESPONSE_TIMEOUT) still contains data then something spam rs485, check node(s) or increase RESPONSE_TIMEOUT
        readErr = SDM_ERR_TIMEOUT; // err debug (4) but returned value may be correct

    if (readErr != SDM_ERR_NO_ERROR) { // if error then copy temp error value to global val and increment global error counter
        readingerrcode = readErr;
        readingerrcount++;
        Serial.printf("SDM error code: %d, error count %u, success count: %u\r\n", readingerrcode, readingerrcount, readingsuccesscount);
    } else {
        ++readingsuccesscount;
    }

#if !defined(USE_POWERMETER_SERIAL2)
    sdmSer.stopListening(); // disable softserial rx interrupt
#endif

    Serial.printf("SDM timing: write: %u, delay: %u, read:%u\r\n", req_end-t_start, resp_start-req_end, millis()-resp_start);

    return (res.value);
}

uint16_t SDM::getErrCode(bool _clear)
{
    uint16_t _tmp = readingerrcode;
    if (_clear == true)
        clearErrCode();
    return (_tmp);
}

uint32_t SDM::getErrCount(bool _clear)
{
    uint32_t _tmp = readingerrcount;
    if (_clear == true)
        clearErrCount();
    return (_tmp);
}

uint32_t SDM::getSuccCount(bool _clear)
{
    uint32_t _tmp = readingsuccesscount;
    if (_clear == true)
        clearSuccCount();
    return (_tmp);
}

void SDM::clearErrCode()
{
    readingerrcode = SDM_ERR_NO_ERROR;
}

void SDM::clearErrCount()
{
    readingerrcount = 0;
}

void SDM::clearSuccCount()
{
    readingsuccesscount = 0;
}

void SDM::setMsTurnaround(uint16_t _msturnaround)
{
    if (_msturnaround < SDM_MIN_DELAY)
        msturnaround = SDM_MIN_DELAY;
    else if (_msturnaround > SDM_MAX_DELAY)
        msturnaround = SDM_MAX_DELAY;
    else
        msturnaround = _msturnaround;
}

void SDM::setMsTimeout(uint16_t _mstimeout)
{
    if (_mstimeout < SDM_MIN_DELAY)
        mstimeout = SDM_MIN_DELAY;
    else if (_mstimeout > SDM_MAX_DELAY)
        mstimeout = SDM_MAX_DELAY;
    else
        mstimeout = _mstimeout;
}

uint16_t SDM::getMsTurnaround()
{
    return (msturnaround);
}

uint16_t SDM::getMsTimeout()
{
    return (mstimeout);
}

uint16_t SDM::calculateCRC(uint8_t* array, uint8_t len)
{
    uint16_t _crc, _flag;
    _crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        _crc ^= (uint16_t)array[i];
        for (uint8_t j = 8; j; j--) {
            _flag = _crc & 0x0001;
            _crc >>= 1;
            if (_flag)
                _crc ^= 0xA001;
        }
    }
    return _crc;
}

void SDM::flush(unsigned long _flushtime)
{
    unsigned long flushstart = millis();
    while (sdmSer.available() && (millis() - flushstart < _flushtime)) {
        if (sdmSer.available()) // read serial if any old data is available
            sdmSer.read();
        delay(1);
    }

    //  Serial.printf("flush timeout %ld\r\n", (millis() - flushstart));
}
