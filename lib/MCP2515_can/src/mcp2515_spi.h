// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __MCP2515_SPI_H__
#define __MCP2515_SPI_H__

#include <Arduino.h>
#include <stdint.h>
#include <driver/spi_master.h>

static const uint32_t MCP2515_SPI_SPEED = 10000000; // 10MHz

enum REGISTER_t {
	MCP_RXF0SIDH = (uint8_t)0x00,
	MCP_RXF0SIDL = (uint8_t)0x01,
	MCP_RXF0EID8 = (uint8_t)0x02,
	MCP_RXF0EID0 = (uint8_t)0x03,
	MCP_RXF1SIDH = (uint8_t)0x04,
	MCP_RXF1SIDL = (uint8_t)0x05,
	MCP_RXF1EID8 = (uint8_t)0x06,
	MCP_RXF1EID0 = (uint8_t)0x07,
	MCP_RXF2SIDH = (uint8_t)0x08,
	MCP_RXF2SIDL = (uint8_t)0x09,
	MCP_RXF2EID8 = (uint8_t)0x0A,
	MCP_RXF2EID0 = (uint8_t)0x0B,
    MCP_BFPCTRL  = (uint8_t)0x0C,
    MCP_TXRTSCTRL= (uint8_t)0x0D,
	MCP_CANSTAT  = (uint8_t)0x0E,
	MCP_CANCTRL  = (uint8_t)0x0F,
	MCP_RXF3SIDH = (uint8_t)0x10,
	MCP_RXF3SIDL = (uint8_t)0x11,
	MCP_RXF3EID8 = (uint8_t)0x12,
	MCP_RXF3EID0 = (uint8_t)0x13,
	MCP_RXF4SIDH = (uint8_t)0x14,
	MCP_RXF4SIDL = (uint8_t)0x15,
	MCP_RXF4EID8 = (uint8_t)0x16,
	MCP_RXF4EID0 = (uint8_t)0x17,
	MCP_RXF5SIDH = (uint8_t)0x18,
	MCP_RXF5SIDL = (uint8_t)0x19,
	MCP_RXF5EID8 = (uint8_t)0x1A,
	MCP_RXF5EID0 = (uint8_t)0x1B,
	MCP_TEC      = (uint8_t)0x1C,
	MCP_REC      = (uint8_t)0x1D,
	MCP_RXM0SIDH = (uint8_t)0x20,
	MCP_RXM0SIDL = (uint8_t)0x21,
	MCP_RXM0EID8 = (uint8_t)0x22,
	MCP_RXM0EID0 = (uint8_t)0x23,
	MCP_RXM1SIDH = (uint8_t)0x24,
	MCP_RXM1SIDL = (uint8_t)0x25,
	MCP_RXM1EID8 = (uint8_t)0x26,
	MCP_RXM1EID0 = (uint8_t)0x27,
	MCP_CNF3     = (uint8_t)0x28,
	MCP_CNF2     = (uint8_t)0x29,
	MCP_CNF1     = (uint8_t)0x2A,
	MCP_CANINTE  = (uint8_t)0x2B,
	MCP_CANINTF  = (uint8_t)0x2C,
	MCP_EFLG     = (uint8_t)0x2D,
	MCP_TXB0CTRL = (uint8_t)0x30,
	MCP_TXB0SIDH = (uint8_t)0x31,
	MCP_TXB0SIDL = (uint8_t)0x32,
	MCP_TXB0EID8 = (uint8_t)0x33,
	MCP_TXB0EID0 = (uint8_t)0x34,
	MCP_TXB0DLC  = (uint8_t)0x35,
	MCP_TXB0DATA = (uint8_t)0x36,
	MCP_TXB1CTRL = (uint8_t)0x40,
	MCP_TXB1SIDH = (uint8_t)0x41,
	MCP_TXB1SIDL = (uint8_t)0x42,
	MCP_TXB1EID8 = (uint8_t)0x43,
	MCP_TXB1EID0 = (uint8_t)0x44,
	MCP_TXB1DLC  = (uint8_t)0x45,
	MCP_TXB1DATA = (uint8_t)0x46,
	MCP_TXB2CTRL = (uint8_t)0x50,
	MCP_TXB2SIDH = (uint8_t)0x51,
	MCP_TXB2SIDL = (uint8_t)0x52,
	MCP_TXB2EID8 = (uint8_t)0x53,
	MCP_TXB2EID0 = (uint8_t)0x54,
	MCP_TXB2DLC  = (uint8_t)0x55,
	MCP_TXB2DATA = (uint8_t)0x56,
	MCP_RXB0CTRL = (uint8_t)0x60,
	MCP_RXB0SIDH = (uint8_t)0x61,
	MCP_RXB0SIDL = (uint8_t)0x62,
	MCP_RXB0EID8 = (uint8_t)0x63,
	MCP_RXB0EID0 = (uint8_t)0x64,
	MCP_RXB0DLC  = (uint8_t)0x65,
	MCP_RXB0DATA = (uint8_t)0x66,
	MCP_RXB1CTRL = (uint8_t)0x70,
	MCP_RXB1SIDH = (uint8_t)0x71,
	MCP_RXB1SIDL = (uint8_t)0x72,
	MCP_RXB1EID8 = (uint8_t)0x73,
	MCP_RXB1EID0 = (uint8_t)0x74,
	MCP_RXB1DLC  = (uint8_t)0x75,
	MCP_RXB1DATA = (uint8_t)0x76
};

enum MCP_BxBFS_MSK_t {
    MCP_BxBFS_MASK = (uint8_t)0x30,
    MCP_BxBFE_MASK = (uint8_t)0x0C,
    MCP_BxBFM_MASK = (uint8_t)0x03,

    MCP_BxRTS_MASK = (uint8_t)0x38,
    MCP_BxRTSM_MASK= (uint8_t)0x07
};

enum INSTRUCTION_t {
	INSTRUCTION_WRITE       = (uint8_t)0x02,
	INSTRUCTION_READ        = (uint8_t)0x03,
	INSTRUCTION_BITMOD      = (uint8_t)0x05,
	INSTRUCTION_LOAD_TX0    = (uint8_t)0x40,
	INSTRUCTION_LOAD_TX1    = (uint8_t)0x42,
	INSTRUCTION_LOAD_TX2    = (uint8_t)0x44,
	INSTRUCTION_RTS_TX0     = (uint8_t)0x81,
	INSTRUCTION_RTS_TX1     = (uint8_t)0x82,
	INSTRUCTION_RTS_TX2     = (uint8_t)0x84,
	INSTRUCTION_RTS_ALL     = (uint8_t)0x87,
	INSTRUCTION_READ_RX0    = (uint8_t)0x90,
	INSTRUCTION_READ_RX1    = (uint8_t)0x94,
	INSTRUCTION_READ_STATUS = (uint8_t)0xA0,
	INSTRUCTION_RX_STATUS   = (uint8_t)0xB0,
	INSTRUCTION_RESET       = (uint8_t)0xC0
};

class MCP2515SPIClass
{
public:
    void spi_init(void);
    void spi_deinit(void);
    void spi_reset(void);

protected:
    uint8_t spi_readRegister(const REGISTER_t reg);
    void spi_readRegisters(const REGISTER_t reg, uint8_t values[], const uint8_t n);
    void spi_setRegister(const REGISTER_t reg, const uint8_t value);
    void spi_setRegisters(const REGISTER_t reg, const uint8_t values[], const uint8_t n);
    void spi_modifyRegister(const REGISTER_t reg, const uint8_t mask, const uint8_t data);
    uint8_t spi_getStatus(void);

    spi_device_handle_t _spi;
    gpio_num_t _pin_irq;

private:
    static bool connection_check_interrupt(gpio_num_t pin_irq);

    SemaphoreHandle_t paramLock = NULL;
};

#endif
