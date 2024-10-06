// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _MCP2515_CAN_H_
#define _MCP2515_CAN_H_

#include "mcp2515_spi.h"
#include "can.h"

#define TAG_MCP2515 "MCP2515"

#define MCP_TIMEOUTVALUE 2500  // In Microseconds, May need changed depending on application and baud rate

/*
 *  Speed 8M
 */
#define MCP_8MHz_1000kBPS_CFG1 (0x00)
#define MCP_8MHz_1000kBPS_CFG2 (0x80)
#define MCP_8MHz_1000kBPS_CFG3 (0x80)

#define MCP_8MHz_500kBPS_CFG1 (0x00)
#define MCP_8MHz_500kBPS_CFG2 (0x90)
#define MCP_8MHz_500kBPS_CFG3 (0x82)

#define MCP_8MHz_250kBPS_CFG1 (0x00)
#define MCP_8MHz_250kBPS_CFG2 (0xB1)
#define MCP_8MHz_250kBPS_CFG3 (0x85)

#define MCP_8MHz_200kBPS_CFG1 (0x00)
#define MCP_8MHz_200kBPS_CFG2 (0xB4)
#define MCP_8MHz_200kBPS_CFG3 (0x86)

#define MCP_8MHz_125kBPS_CFG1 (0x01)
#define MCP_8MHz_125kBPS_CFG2 (0xB1)
#define MCP_8MHz_125kBPS_CFG3 (0x85)

#define MCP_8MHz_100kBPS_CFG1 (0x01)
#define MCP_8MHz_100kBPS_CFG2 (0xB4)
#define MCP_8MHz_100kBPS_CFG3 (0x86)

#define MCP_8MHz_80kBPS_CFG1 (0x01)
#define MCP_8MHz_80kBPS_CFG2 (0xBF)
#define MCP_8MHz_80kBPS_CFG3 (0x87)

#define MCP_8MHz_50kBPS_CFG1 (0x03)
#define MCP_8MHz_50kBPS_CFG2 (0xB4)
#define MCP_8MHz_50kBPS_CFG3 (0x86)

#define MCP_8MHz_40kBPS_CFG1 (0x03)
#define MCP_8MHz_40kBPS_CFG2 (0xBF)
#define MCP_8MHz_40kBPS_CFG3 (0x87)

#define MCP_8MHz_33k3BPS_CFG1 (0x47)
#define MCP_8MHz_33k3BPS_CFG2 (0xE2)
#define MCP_8MHz_33k3BPS_CFG3 (0x85)

#define MCP_8MHz_31k25BPS_CFG1 (0x07)
#define MCP_8MHz_31k25BPS_CFG2 (0xA4)
#define MCP_8MHz_31k25BPS_CFG3 (0x84)

#define MCP_8MHz_20kBPS_CFG1 (0x07)
#define MCP_8MHz_20kBPS_CFG2 (0xBF)
#define MCP_8MHz_20kBPS_CFG3 (0x87)

#define MCP_8MHz_10kBPS_CFG1 (0x0F)
#define MCP_8MHz_10kBPS_CFG2 (0xBF)
#define MCP_8MHz_10kBPS_CFG3 (0x87)

#define MCP_8MHz_5kBPS_CFG1 (0x1F)
#define MCP_8MHz_5kBPS_CFG2 (0xBF)
#define MCP_8MHz_5kBPS_CFG3 (0x87)

/*
 *  speed 16M
 */
#define MCP_16MHz_1000kBPS_CFG1 (0x00)
#define MCP_16MHz_1000kBPS_CFG2 (0xD0)
#define MCP_16MHz_1000kBPS_CFG3 (0x82)

#define MCP_16MHz_500kBPS_CFG1 (0x00)
#define MCP_16MHz_500kBPS_CFG2 (0xF0)
#define MCP_16MHz_500kBPS_CFG3 (0x86)

#define MCP_16MHz_250kBPS_CFG1 (0x41)
#define MCP_16MHz_250kBPS_CFG2 (0xF1)
#define MCP_16MHz_250kBPS_CFG3 (0x85)

#define MCP_16MHz_200kBPS_CFG1 (0x01)
#define MCP_16MHz_200kBPS_CFG2 (0xFA)
#define MCP_16MHz_200kBPS_CFG3 (0x87)

#define MCP_16MHz_125kBPS_CFG1 (0x03)
#define MCP_16MHz_125kBPS_CFG2 (0xF0)
#define MCP_16MHz_125kBPS_CFG3 (0x86)

#define MCP_16MHz_100kBPS_CFG1 (0x03)
#define MCP_16MHz_100kBPS_CFG2 (0xFA)
#define MCP_16MHz_100kBPS_CFG3 (0x87)

#define MCP_16MHz_80kBPS_CFG1 (0x03)
#define MCP_16MHz_80kBPS_CFG2 (0xFF)
#define MCP_16MHz_80kBPS_CFG3 (0x87)

#define MCP_16MHz_83k3BPS_CFG1 (0x03)
#define MCP_16MHz_83k3BPS_CFG2 (0xBE)
#define MCP_16MHz_83k3BPS_CFG3 (0x07)

#define MCP_16MHz_50kBPS_CFG1 (0x07)
#define MCP_16MHz_50kBPS_CFG2 (0xFA)
#define MCP_16MHz_50kBPS_CFG3 (0x87)

#define MCP_16MHz_40kBPS_CFG1 (0x07)
#define MCP_16MHz_40kBPS_CFG2 (0xFF)
#define MCP_16MHz_40kBPS_CFG3 (0x87)

#define MCP_16MHz_33k3BPS_CFG1 (0x4E)
#define MCP_16MHz_33k3BPS_CFG2 (0xF1)
#define MCP_16MHz_33k3BPS_CFG3 (0x85)

#define MCP_16MHz_20kBPS_CFG1 (0x0F)
#define MCP_16MHz_20kBPS_CFG2 (0xFF)
#define MCP_16MHz_20kBPS_CFG3 (0x87)

#define MCP_16MHz_10kBPS_CFG1 (0x1F)
#define MCP_16MHz_10kBPS_CFG2 (0xFF)
#define MCP_16MHz_10kBPS_CFG3 (0x87)

#define MCP_16MHz_5kBPS_CFG1 (0x3F)
#define MCP_16MHz_5kBPS_CFG2 (0xFF)
#define MCP_16MHz_5kBPS_CFG3 (0x87)

/*
 *  speed 20M
 */
#define MCP_20MHz_1000kBPS_CFG1 (0x00)
#define MCP_20MHz_1000kBPS_CFG2 (0xD9)
#define MCP_20MHz_1000kBPS_CFG3 (0x82)

#define MCP_20MHz_500kBPS_CFG1 (0x00)
#define MCP_20MHz_500kBPS_CFG2 (0xFA)
#define MCP_20MHz_500kBPS_CFG3 (0x87)

#define MCP_20MHz_250kBPS_CFG1 (0x41)
#define MCP_20MHz_250kBPS_CFG2 (0xFB)
#define MCP_20MHz_250kBPS_CFG3 (0x86)

#define MCP_20MHz_200kBPS_CFG1 (0x01)
#define MCP_20MHz_200kBPS_CFG2 (0xFF)
#define MCP_20MHz_200kBPS_CFG3 (0x87)

#define MCP_20MHz_125kBPS_CFG1 (0x03)
#define MCP_20MHz_125kBPS_CFG2 (0xFA)
#define MCP_20MHz_125kBPS_CFG3 (0x87)

#define MCP_20MHz_100kBPS_CFG1 (0x04)
#define MCP_20MHz_100kBPS_CFG2 (0xFA)
#define MCP_20MHz_100kBPS_CFG3 (0x87)

#define MCP_20MHz_83k3BPS_CFG1 (0x04)
#define MCP_20MHz_83k3BPS_CFG2 (0xFE)
#define MCP_20MHz_83k3BPS_CFG3 (0x87)

#define MCP_20MHz_80kBPS_CFG1 (0x04)
#define MCP_20MHz_80kBPS_CFG2 (0xFF)
#define MCP_20MHz_80kBPS_CFG3 (0x87)

#define MCP_20MHz_50kBPS_CFG1 (0x09)
#define MCP_20MHz_50kBPS_CFG2 (0xFA)
#define MCP_20MHz_50kBPS_CFG3 (0x87)

#define MCP_20MHz_40kBPS_CFG1 (0x09)
#define MCP_20MHz_40kBPS_CFG2 (0xFF)
#define MCP_20MHz_40kBPS_CFG3 (0x87)

#define MCP_20MHz_33k3BPS_CFG1 (0x0B)
#define MCP_20MHz_33k3BPS_CFG2 (0xFF)
#define MCP_20MHz_33k3BPS_CFG3 (0x87)

static const uint8_t CANSTAT_OPMOD = 0xE0;
static const uint8_t CANSTAT_ICOD = 0x0E;

static const uint8_t CNF3_SOF = 0x80;

static const uint8_t TXB_EXIDE_MASK = 0x08;
static const uint8_t DLC_MASK       = 0x0F;
static const uint8_t RTR_MASK       = 0x40;

static const uint8_t RXBnCTRL_RXM_ANY    = 0x60;
static const uint8_t RXBnCTRL_RXM_STD    = 0x20;
static const uint8_t RXBnCTRL_RXM_EXT    = 0x40;
static const uint8_t RXBnCTRL_RXM_STDEXT = 0x00;
static const uint8_t RXBnCTRL_RXM_MASK   = 0x60;
static const uint8_t RXBnCTRL_RTR        = 0x08;
static const uint8_t RXB0CTRL_BUKT       = 0x04;
static const uint8_t RXB0CTRL_FILHIT_MASK = 0x03;
static const uint8_t RXB1CTRL_FILHIT_MASK = 0x07;
static const uint8_t RXB0CTRL_FILHIT = 0x00;
static const uint8_t RXB1CTRL_FILHIT = 0x01;

static const uint8_t MCP_SIDH = 0;
static const uint8_t MCP_SIDL = 1;
static const uint8_t MCP_EID8 = 2;
static const uint8_t MCP_EID0 = 3;
static const uint8_t MCP_DLC  = 4;
static const uint8_t MCP_DATA = 5;

static const uint8_t CANCTRL_REQOP = 0xE0;
static const uint8_t CANCTRL_ABAT = 0x10;
static const uint8_t CANCTRL_OSM = 0x08;
static const uint8_t CANCTRL_CLKEN = 0x04;
static const uint8_t CANCTRL_CLKPRE = 0x03;

#define N_TXBUFFERS 3
#define N_RXBUFFERS 2

#define MCP_STDEXT   0                                                  /* Standard and Extended        */
#define MCP_STD      1                                                  /* Standard IDs ONLY            */
#define MCP_EXT      2                                                  /* Extended IDs ONLY            */
#define MCP_ANY      3                                                  /* Disables Masks and Filters   */

enum MCP2515_CLOCK_t {
    MCP_20MHZ,
    MCP_16MHZ,
    MCP_8MHZ
};

enum CAN_SPEED_t {
    CAN_5KBPS,
    CAN_10KBPS,
    CAN_20KBPS,
    CAN_31K25BPS,
    CAN_33KBPS,
    CAN_40KBPS,
    CAN_50KBPS,
    CAN_80KBPS,
    CAN_83K3BPS,
    CAN_95KBPS,
    CAN_100KBPS,
    CAN_125KBPS,
    CAN_200KBPS,
    CAN_250KBPS,
    CAN_500KBPS,
    CAN_1000KBPS
};

enum CAN_CLKOUT_t {
    CLKOUT_DISABLE = -1,
    CLKOUT_DIV1 = 0x0,
    CLKOUT_DIV2 = 0x1,
    CLKOUT_DIV4 = 0x2,
    CLKOUT_DIV8 = 0x3,
};

enum ERROR_t {
	CAN_OK             = 0,
    CAN_FAILINIT       = 1,
	CAN_FAILTX         = 2,
	CAN_FAILTX_DLC     = (2 | 0x80),
    CAN_MSGAVAIL       = 3,
	CAN_NOMSG          = 4,
    CAN_CTRLERROR      = 5,
    CAN_GETTXBFTIMEOUT = 6,
    CAN_SENDMSGTIMEOUT = 7,
    CAN_FAILRX         = 8,
	CAN_FAILRX_DLC     = (8 | 0x80),
	CAN_ALLTXBUSY      = 0x80,
	CAN_FAIL           = 0xFF,
};

enum MASK_t {
	MASK0,
	MASK1
};

enum RXF_t {
	RXF0 = 0,
	RXF1 = 1,
	RXF2 = 2,
	RXF3 = 3,
	RXF4 = 4,
	RXF5 = 5
};

enum CANINTF_t {
	CANINTF_RX0IF = 0x01,
	CANINTF_RX1IF = 0x02,
	CANINTF_TX0IF = 0x04,
	CANINTF_TX1IF = 0x08,
	CANINTF_TX2IF = 0x10,
	CANINTF_ERRIF = 0x20,
	CANINTF_WAKIF = 0x40,
	CANINTF_MERRF = 0x80
};

enum EFLG_t {
	EFLG_RX1OVR = (uint8_t)0b10000000,
	EFLG_RX0OVR = (uint8_t)0b01000000,
	EFLG_TXBO   = (uint8_t)0b00100000,
	EFLG_TXEP   = (uint8_t)0b00010000,
	EFLG_RXEP   = (uint8_t)0b00001000,
	EFLG_TXWAR  = (uint8_t)0b00000100,
	EFLG_RXWAR  = (uint8_t)0b00000010,
	EFLG_EWARN  = (uint8_t)0b00000001
};
static const uint8_t EFLG_ERRORMASK = EFLG_RX1OVR
									| EFLG_RX0OVR
									| EFLG_TXBO
									| EFLG_TXEP
									| EFLG_RXEP;


enum CANCTRL_REQOP_MODE_t {
	MCP_NORMAL     = (uint8_t)0x00,
    MODE_ONESHOT   = (uint8_t)0x08,
    ABORT_TX       = (uint8_t)0x10,
	MCP_SLEEP      = (uint8_t)0x20,
	MCP_LOOPBACK   = (uint8_t)0x40,
	MCP_LISTENONLY = (uint8_t)0x60,
	MODE_CONFIG    = (uint8_t)0x80,
	MODE_POWERUP   = (uint8_t)0xE0,
};
static const uint8_t MODE_MASK = MCP_SLEEP | MCP_LOOPBACK | MCP_LISTENONLY | MODE_CONFIG;

enum STAT_t {
	STAT_RX0IF     = (uint8_t)(1<<0),
	STAT_RX1IF     = (uint8_t)(1<<1)
};
static const uint8_t STAT_RXIF_MASK = STAT_RX0IF | STAT_RX1IF;

enum TXBnCTRL_t {
	TXB_ABTF   = (uint8_t)0x40,
	TXB_MLOA   = (uint8_t)0x20,
	TXB_TXERR  = (uint8_t)0x10,
	TXB_TXREQ  = (uint8_t)0x08,
	TXB_TXIE   = (uint8_t)0x04,
	TXB_TXP    = (uint8_t)0x03
};

static const uint32_t MCP2515_SPI_SPEED = 10000000; // 10MHz

class MCP2515Class : public MCP2515SPIClass
{
public:
    MCP2515Class(const uint8_t pin_miso, const uint8_t pin_mosi, const uint8_t pin_clk, const uint8_t pin_cs, const uint8_t pin_irq, const uint32_t _spi_speed = MCP2515_SPI_SPEED);
    MCP2515Class& operator=(const MCP2515Class&) = delete;
    ~MCP2515Class();

    uint8_t initMCP2515(const int8_t canIDMode, const CAN_SPEED_t canSpeed = CAN_500KBPS, const MCP2515_CLOCK_t canClock = MCP_16MHZ);
    uint8_t reset(const int8_t canIDMode, const CAN_SPEED_t canSpeed, const MCP2515_CLOCK_t canClock);
    bool isInterrupt(void);

    void setSleepWakeup(const bool enable);
    uint8_t setMode(const CANCTRL_REQOP_MODE_t mode);

    uint8_t setOneShotMode(bool set);
    uint8_t setClkOut(const CAN_CLKOUT_t divisor);
    uint8_t setFilterMask(const MASK_t num, const bool ext, const uint32_t ulData);
    uint8_t setFilter(const RXF_t num, const bool ext, const uint32_t ulData);
    uint8_t sendMsgBuf(can_message_t *tx_message);
    uint8_t readMsgBuf(can_message_t *rx_message);
    uint8_t checkReceive(void);
    uint8_t checkError(void);
    uint8_t errorCountRX(void);
    uint8_t errorCountTX(void);
    uint8_t getErrorFlags(void);
    void clearRXnOVRFlags(void);
    uint8_t getInterrupts(void);
    uint8_t getInterruptMask(void);
    void clearInterrupts(void);
    void clearTXInterrupts(void);
    void clearRXnOVR(void);
    void clearMERR();
    void clearERRIF();
    uint8_t abortTX(void);     // Abort queued transmission(s)
    uint8_t setGPO(uint8_t data);  // Sets GPO
    uint8_t getGPI(void);        // Reads GPI

private:
    enum RXBn_t { RXB0 = 0, RXB1 = 1 };
    enum TXBn_t { TXB0 = 0, TXB1 = 1, TXB2 = 2 };

    uint8_t initCANBuffers(void);
    uint8_t requestNewMode(const uint8_t newmode);

    uint8_t setBitrate(const CAN_SPEED_t canSpeed, const MCP2515_CLOCK_t mcp2515Clock);

    uint8_t sendMessage(const TXBn_t txbn, can_message_t *tx_message);
    uint8_t readMessage(const RXBn_t rxbn, can_message_t *rx_message);

    void prepareId(uint8_t *buffer, const bool ext, const uint32_t id);

    struct TXBn_REGS_t {
	    REGISTER_t CTRL;
    	REGISTER_t SIDH;
	    REGISTER_t DATA;
    };
    struct RXBn_REGS_t {
	    REGISTER_t CTRL;
    	REGISTER_t SIDH;
	    REGISTER_t DATA;
	    CANINTF_t  CANINTF_RXnIF;
    };

	TXBn_REGS_t* TXB_ptr = NULL;
	RXBn_REGS_t* RXB_ptr = NULL;

    int8_t _pin_miso;
    int8_t _pin_mosi;
    int8_t _pin_clk;
    int8_t _pin_cs;
    int8_t _pin_irq;
    uint32_t _spi_speed;
};

#endif
