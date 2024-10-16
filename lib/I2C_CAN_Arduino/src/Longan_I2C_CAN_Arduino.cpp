#include "Longan_I2C_CAN_Arduino.h"


I2C_CAN::I2C_CAN(TwoWire *wire, unsigned char __addr, int8_t __scl, int8_t __sda, uint32_t __frequency)
{
    IIC_ADDR = __addr;
    m_sda = __sda;
    m_scl = __scl;
    m_frequency = __frequency;
    if (wire == NULL) wire = &Wire;
    __wire = wire;
}

void I2C_CAN::begin()
{
    __wire->begin(m_scl, m_sda, m_frequency);
}

void I2C_CAN::IIC_CAN_SetReg(unsigned char __reg, unsigned char __len, unsigned char *__dta)
{
    __wire->beginTransmission(IIC_ADDR);
    __wire->write(__reg);
    for(int i=0; i<__len; i++)
    {
        __wire->write(__dta[i]);
    }
    __wire->endTransmission();
}

void I2C_CAN::IIC_CAN_SetReg(unsigned char __reg, unsigned char __dta)
{
    __wire->beginTransmission(IIC_ADDR);
    __wire->write(__reg);
    __wire->write(__dta);
    __wire->endTransmission();
}

bool I2C_CAN::IIC_CAN_GetReg(unsigned char __reg, unsigned char *__dta)
{
    __wire->beginTransmission(IIC_ADDR);
    __wire->write(__reg);
    __wire->endTransmission();
    __wire->requestFrom((int)IIC_ADDR, 1);

    while(__wire->available())
    {
        *__dta = __wire->read();
        return 1;
    }

    return 0;
}

bool I2C_CAN::IIC_CAN_GetReg(unsigned char __reg, int len, unsigned char *__dta)
{
    __wire->beginTransmission(IIC_ADDR);
    __wire->write(__reg);
    __wire->endTransmission();
    __wire->requestFrom((int)IIC_ADDR, len);

    int __len = 0;

    while(__wire->available())
    {
        __dta[__len++] = __wire->read();
    }

    return (len == __len);
}


byte I2C_CAN::begin(byte speedset)                                      // init can
{
    __wire->begin(m_sda, m_scl, m_frequency);

    IIC_CAN_SetReg(REG_BAUD, speedset);
    delay(10);

    unsigned char __speed = 0;

    if(IIC_CAN_GetReg(REG_BAUD, &__speed))
    {
        if(speedset == __speed)return 1;

    }

    delay(100);
    return 0;
}

byte I2C_CAN::init_Mask(byte num, byte ext, unsigned long ulData)       // init Masks
{
    unsigned char dta[5];

    dta[0] = ext;
    dta[1] = 0xff & (ulData >> 24);
    dta[2] = 0xff & (ulData >> 16);
    dta[3] = 0xff & (ulData >> 8);
    dta[4] = 0xff & (ulData >> 0);

    unsigned char mask = (num == 0) ? REG_MASK0 : REG_MASK1;

    IIC_CAN_SetReg(mask, 5, dta);
    delay(50);

    return CAN_OK;
}

byte I2C_CAN::init_Filt(byte num, byte ext, unsigned long ulData)       // init filters
{
    unsigned char dta[5];

    dta[0] = ext;
    dta[1] = 0xff & (ulData >> 24);
    dta[2] = 0xff & (ulData >> 16);
    dta[3] = 0xff & (ulData >> 8);
    dta[4] = 0xff & (ulData >> 0);

    unsigned char filt = (7+num)*0x10;

    IIC_CAN_SetReg(filt, 5, dta);
    delay(50);

    return CAN_OK;
}

byte I2C_CAN::sendMsgBuf(unsigned long id, byte ext, byte rtr, byte len, byte *buf)     // send buf
{
    unsigned char dta[16];

    dta[0] = 0xff & (id >> 24);
    dta[1] = 0xff & (id >> 16);
    dta[2] = 0xff & (id >> 8);
    dta[3] = 0xff & (id >> 0);

    dta[4] = ext;
    dta[5] = rtr;

    dta[6] = len;

    for(int i=0; i<len; i++)
    {
        dta[7+i] = buf[i];
    }

    dta[15] = makeCheckSum(dta, 15);

    IIC_CAN_SetReg(REG_SEND, 16, dta);

    return CAN_OK;
}

byte I2C_CAN::sendMsgBuf(unsigned long id, byte ext, byte len, byte *buf)               // send buf
{
    return sendMsgBuf(id, ext, 0, len, buf);
}

byte I2C_CAN::readMsgBuf(byte *len, byte *buf)                          // read buf
{
    if (readMsgBufID(&m_ID, len, buf)) return CAN_OK;
    else return CAN_NOMSG;
}

byte I2C_CAN::readMsgBufID(unsigned long *ID, byte *len, byte *buf)     // read buf with object ID
{
    //IIC_CAN_GetReg(unsigned char __reg, int len, unsigned char *__dta)
    unsigned long id = 0;

    unsigned char dta[16];

    IIC_CAN_GetReg(REG_RECV, 16, dta);

    unsigned char __checksum = makeCheckSum(dta, 15);

    if(__checksum == dta[15])           // checksum ok
    {
        id = dta[0];
        id <<= 8;
        id += dta[1];
        id <<= 8;
        id += dta[2];
        id <<= 8;
        id += dta[3];

        *ID = id;

        m_ID  = id;
        m_EXT = dta[4];
        m_RTR = dta[5];

        *len = dta[6];

        //Serial.print("readMsgBufID, len = ");
        //Serial.println(*len);

        if(*len > 8)return 0;

        for(int i=0; i<*len; i++)
            buf[i] = dta[7+i];

        return 1;
    }
    else
    {
        //Serial.println("CHECKSUM ERROR");
        return 0;
    }
}

byte I2C_CAN::checkReceive(void)                                        // if something received
{
    unsigned char num = 0;

    if(IIC_CAN_GetReg(REG_DNUM, &num))
    {
        if(num > 0)
        {
            return CAN_MSGAVAIL;
        }
    }

    return 0;
}

byte I2C_CAN::checkError(void)                                          // if something error
{
    return 0;
}

unsigned long I2C_CAN::getCanId(void)                                   // get can id when receive
{
    return m_ID;
}

byte I2C_CAN::isRemoteRequest(void)                                     // get RR flag when receive
{
    return m_RTR;
}

byte I2C_CAN::isExtendedFrame(void)                                     // did we recieve 29bit frame?
{
    return m_EXT;
}

unsigned char I2C_CAN::makeCheckSum(unsigned char *dta, int len)
{
    unsigned long sum = 0;
    for(int i=0; i<len; i++)sum += dta[i];

    if(sum > 0xff)
    {
        sum = ~sum;
        sum += 1;
    }

    sum  = sum & 0xff;
    return sum;
}

// END FILE
