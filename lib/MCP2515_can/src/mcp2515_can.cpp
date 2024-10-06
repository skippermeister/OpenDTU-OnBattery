// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mcp2515_can.h"

MCP2515Class::MCP2515Class(const uint8_t pin_miso, const uint8_t pin_mosi, const uint8_t pin_clk, const uint8_t pin_cs, const uint8_t pin_irq, const uint32_t spi_speed)
{
    _pin_miso = pin_miso;
    _pin_mosi = pin_mosi;
    _pin_clk = pin_clk;
    _pin_cs = pin_cs;
    _pin_irq = pin_irq;
    _spi_speed = spi_speed;
}

MCP2515Class::~MCP2515Class()
{
    // TODO(skippermeister): support cleanup at some point?

    spi_deinit();

    if (TXB_ptr) free(TXB_ptr);
    if (RXB_ptr) free(RXB_ptr);
}

uint8_t MCP2515Class::initMCP2515(const int8_t canIDMode, const CAN_SPEED_t canSpeed, const MCP2515_CLOCK_t canClock)
{
    spi_init(_pin_miso, _pin_mosi, _pin_clk, _pin_cs, _pin_irq, _spi_speed);

	TXB_ptr = (TXBn_REGS_t*)malloc(sizeof(TXBn_REGS_t[N_TXBUFFERS]));
	RXB_ptr = (RXBn_REGS_t*)malloc(sizeof(RXBn_REGS_t[N_RXBUFFERS]));
	if(TXB_ptr == NULL || RXB_ptr == NULL){
		ESP_LOGE(TAG_MCP2515, "Couldn't initialize (TXB_ptr || RXB_ptr). (NULL pointer)");
		return CAN_FAILINIT;
	}

	// TXBn and RXBn REGISTER INITIALIZATION
	TXB_ptr[0].CTRL = MCP_TXB0CTRL;
	TXB_ptr[0].DATA = MCP_TXB0DATA;
	TXB_ptr[0].SIDH = MCP_TXB0SIDH;

	TXB_ptr[1].CTRL = MCP_TXB1CTRL;
	TXB_ptr[1].DATA = MCP_TXB1DATA;
	TXB_ptr[1].SIDH = MCP_TXB1SIDH;

	TXB_ptr[2].CTRL = MCP_TXB2CTRL;
	TXB_ptr[2].DATA = MCP_TXB2DATA;
	TXB_ptr[2].SIDH = MCP_TXB2SIDH;

	RXB_ptr[0].CTRL = MCP_RXB0CTRL;
	RXB_ptr[0].DATA = MCP_RXB0DATA;
	RXB_ptr[0].SIDH = MCP_RXB0SIDH;
	RXB_ptr[0].CANINTF_RXnIF = CANINTF_RX0IF;

	RXB_ptr[1].CTRL = MCP_RXB1CTRL;
	RXB_ptr[1].DATA = MCP_RXB1DATA;
	RXB_ptr[1].SIDH = MCP_RXB1SIDH;
	RXB_ptr[1].CANINTF_RXnIF = CANINTF_RX1IF;

    return reset(canIDMode, canSpeed, canClock);
}

uint8_t MCP2515Class::initCANBuffers(void)
{
    // clear filters and masks
    // do not filter any standard frames for RXF0 used by RXB0
    // do not filter any extended frames for RXF1 used by RXB1
    const RXF_t filters[] = {RXF0, RXF1, RXF2, RXF3, RXF4, RXF5};
    for (int i=0; i<6; i++) {
        const bool ext = (i == 1);
        int rc = setFilter(filters[i], ext, 0);
        if (rc != CAN_OK) {
            return rc;
        }
    }

    MASK_t masks[] = {MASK0, MASK1};
    for (int i=0; i<2; i++) {
        int rc = setFilterMask(masks[i], true, 0);
        if (rc != CAN_OK) {
            return rc;
        }
    }

    /* Clear, deactivate the three transmit buffers
     * TXBnCTRL -> TXBnD7
     */
    uint8_t zeros[14];
    memset(zeros, 0, sizeof(zeros));
    spi_setRegisters(MCP_TXB0CTRL, zeros, 14);
    spi_setRegisters(MCP_TXB1CTRL, zeros, 14);
    spi_setRegisters(MCP_TXB2CTRL, zeros, 14);

    spi_setRegister(MCP_RXB0CTRL, 0);
    spi_setRegister(MCP_RXB1CTRL, 0);

    return CAN_OK;
}

uint8_t MCP2515Class::reset(const int8_t canIDMode, const CAN_SPEED_t canSpeed, const MCP2515_CLOCK_t canClock)
{
    spi_reset();

    CANCTRL_REQOP_MODE_t mcpMode = MCP_LOOPBACK;

    if (setMode(MODE_CONFIG) != CAN_OK)
    {
        Serial.println(F("Entering Configuration Mode Failure..."));
        return CAN_FAILINIT;
    }
    Serial.println(F("Entering Configuration Mode Successful!"));

    vTaskDelay(10 / portTICK_PERIOD_MS);
    // Set Baudrate
    if(setBitrate(canSpeed, canClock) != CAN_OK)
    {
        Serial.println("Setting Baudrate Failure...");
        return CAN_FAILINIT;
    }
    Serial.println("Setting Baudrate Successful!");

    if (initCANBuffers() != CAN_OK) {
        Serial.println(F("Initializing CAN buffers Failure..."));
        return CAN_FAILINIT;
    }

    // spi_setRegister(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF | CANINTF_ERRIF | CANINTF_MERRF);
    spi_setRegister(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF);

    //Sets BF pins as GPO
    spi_setRegister(MCP_BFPCTRL, MCP_BxBFS_MASK | MCP_BxBFE_MASK);
    //Sets RTS pins as GPI
    spi_setRegister(MCP_TXRTSCTRL, 0x00);

    switch(canIDMode)
    {
        case (MCP_ANY):
            spi_modifyRegister(MCP_RXB0CTRL,
                    RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT,
                    RXBnCTRL_RXM_ANY | RXB0CTRL_BUKT);
            spi_modifyRegister(MCP_RXB1CTRL, RXBnCTRL_RXM_MASK, RXBnCTRL_RXM_ANY);
            break;

         case (MCP_STDEXT):
            // receives all valid messages using either Standard or Extended Identifiers that
            // meet filter criteria. RXF0 is applied for RXB0, RXF1 is applied for RXB1
            spi_modifyRegister(MCP_RXB0CTRL,
                    RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT | RXB0CTRL_FILHIT_MASK,
                    RXBnCTRL_RXM_STDEXT | RXB0CTRL_BUKT | RXB0CTRL_FILHIT);
            spi_modifyRegister(MCP_RXB1CTRL,
                    RXBnCTRL_RXM_MASK | RXB1CTRL_FILHIT_MASK,
                    RXBnCTRL_RXM_STDEXT | RXB1CTRL_FILHIT);
            break;

        default:
            Serial.println("Setting ID Mode Failure...");
            return CAN_FAILINIT;
    }

    if (setMode(mcpMode) != CAN_OK)
    {
          Serial.println(F("Returning to Previous Mode Failure..."));
          return CAN_FAILINIT;
    }

    return CAN_OK;
}

bool MCP2515Class::isInterrupt(void)
{
    return !digitalRead(_pin_irq);
}

uint8_t MCP2515Class::setOneShotMode(bool set)
{
    uint8_t data = 0;
    if (set)
        data = MODE_ONESHOT;
    spi_modifyRegister(MCP_CANCTRL, MODE_ONESHOT, data);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    bool modeMatch = false;
    for (int i = 0; i < 10; i++) {
        uint8_t ctrlR = spi_readRegister(MCP_CANCTRL);
        modeMatch = (ctrlR & (1U << 3)) == data;
        if (modeMatch)
            break;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    return modeMatch ? CAN_OK : CAN_FAIL;
}

/*********************************************************************************************************
** Function name:           mcp2515_abortTX
** Descriptions:            Aborts any queued transmissions
*********************************************************************************************************/
uint8_t MCP2515Class::abortTX(void)
{
    spi_modifyRegister(MCP_CANCTRL, ABORT_TX, ABORT_TX);

    // Maybe check to see if the TX buffer transmission request bits are cleared instead?
    if((spi_readRegister(MCP_CANCTRL) & ABORT_TX) != ABORT_TX)
	    return CAN_FAIL;
    else
	    return CAN_OK;
}

/*********************************************************************************************************
** Function name:           setGPO
** Descriptions:            Public function, Checks for r
*********************************************************************************************************/
uint8_t MCP2515Class::setGPO(uint8_t data)
{
    spi_modifyRegister(MCP_BFPCTRL, MCP_BxBFS_MASK, (data<<4));

    return 0;
}

/*********************************************************************************************************
** Function name:           getGPI
** Descriptions:            Public function, Checks for r
*********************************************************************************************************/
uint8_t MCP2515Class::getGPI(void)
{
    uint8_t res;
    res = spi_readRegister(MCP_TXRTSCTRL) & MCP_BxRTS_MASK;
    return (res >> 3);
}

void MCP2515Class::setSleepWakeup(const bool enable)
{
    spi_modifyRegister(MCP_CANINTE, CANINTF_WAKIF, enable ? CANINTF_WAKIF: 0);
}

uint8_t MCP2515Class::setMode(const CANCTRL_REQOP_MODE_t mode)
{
	// If the chip is asleep and we want to change mode then a manual wake needs to be done
	// This is done by setting the wake up interrupt flag
	// This undocumented trick was found at https://github.com/mkleemann/can/blob/master/can_sleep_mcp2515.c
	if((spi_readRegister(MCP_CANSTAT) & MODE_MASK) == MCP_SLEEP && mode != MCP_SLEEP)
	{
		// Make sure wake interrupt is enabled
		byte wakeIntEnabled = (spi_readRegister(MCP_CANINTE) & CANINTF_WAKIF);
		if(!wakeIntEnabled)
			spi_modifyRegister(MCP_CANINTE, CANINTF_WAKIF, CANINTF_WAKIF);

		// Set wake flag (this does the actual waking up)
		spi_modifyRegister(MCP_CANINTF, CANINTF_WAKIF, CANINTF_WAKIF);

		// Wait for the chip to exit SLEEP and enter LISTENONLY mode.

		// If the chip is not connected to a CAN bus (or the bus has no other powered nodes) it will sometimes trigger the wake interrupt as soon
		// as it's put to sleep, but it will stay in SLEEP mode instead of automatically switching to LISTENONLY mode.
		// In this situation the mode needs to be manually set to LISTENONLY.

		if(requestNewMode(MCP_LISTENONLY) != CAN_OK)
			return CAN_FAIL;

		// Turn wake interrupt back off if it was originally off
		if(!wakeIntEnabled)
			spi_modifyRegister(MCP_CANINTE, CANINTF_WAKIF, 0);

    }


	// Clear wake flag
	spi_modifyRegister(MCP_CANINTF, CANINTF_WAKIF, 0);

	return requestNewMode(mode);
}

/*********************************************************************************************************
** Function name:           requestNewMode
** Descriptions:            Set control mode
*********************************************************************************************************/
uint8_t MCP2515Class::requestNewMode(const uint8_t newmode)
{
	byte startTime = millis();

	// Spam new mode request and wait for the operation  to complete
	while(1)
	{
		// Request new mode
		// This is inside the loop as sometimes requesting the new mode once doesn't work (usually when attempting to sleep)
		spi_modifyRegister(MCP_CANCTRL, CANSTAT_OPMOD, newmode);

		byte statReg = spi_readRegister(MCP_CANSTAT);
		if((statReg & MODE_MASK) == newmode) // We're now in the new mode
			return CAN_OK;
		else if((byte)(millis() - startTime) > 200) // Wait no more than 200ms for the operation to complete
			return CAN_FAIL;
	}
}

uint8_t MCP2515Class::setBitrate(const CAN_SPEED_t canSpeed, MCP2515_CLOCK_t mcp2515Clock)
{
    int rc = setMode(MODE_CONFIG);
    if (rc != CAN_OK) {
        return rc;
    }

    uint8_t set, cfg1, cfg2, cfg3;
    set = 1;
    switch (mcp2515Clock)
    {
        case (MCP_8MHZ):
        switch (canSpeed)
        {
            case (CAN_5KBPS):                                               //   5KBPS
            cfg1 = MCP_8MHz_5kBPS_CFG1;
            cfg2 = MCP_8MHz_5kBPS_CFG2;
            cfg3 = MCP_8MHz_5kBPS_CFG3;
            break;

            case (CAN_10KBPS):                                              //  10KBPS
            cfg1 = MCP_8MHz_10kBPS_CFG1;
            cfg2 = MCP_8MHz_10kBPS_CFG2;
            cfg3 = MCP_8MHz_10kBPS_CFG3;
            break;

            case (CAN_20KBPS):                                              //  20KBPS
            cfg1 = MCP_8MHz_20kBPS_CFG1;
            cfg2 = MCP_8MHz_20kBPS_CFG2;
            cfg3 = MCP_8MHz_20kBPS_CFG3;
            break;

            case (CAN_31K25BPS):                                            //  31.25KBPS
            cfg1 = MCP_8MHz_31k25BPS_CFG1;
            cfg2 = MCP_8MHz_31k25BPS_CFG2;
            cfg3 = MCP_8MHz_31k25BPS_CFG3;
            break;

            case (CAN_33KBPS):                                              //  33.333KBPS
            cfg1 = MCP_8MHz_33k3BPS_CFG1;
            cfg2 = MCP_8MHz_33k3BPS_CFG2;
            cfg3 = MCP_8MHz_33k3BPS_CFG3;
            break;

            case (CAN_40KBPS):                                              //  40Kbps
            cfg1 = MCP_8MHz_40kBPS_CFG1;
            cfg2 = MCP_8MHz_40kBPS_CFG2;
            cfg3 = MCP_8MHz_40kBPS_CFG3;
            break;

            case (CAN_50KBPS):                                              //  50Kbps
            cfg1 = MCP_8MHz_50kBPS_CFG1;
            cfg2 = MCP_8MHz_50kBPS_CFG2;
            cfg3 = MCP_8MHz_50kBPS_CFG3;
            break;

            case (CAN_80KBPS):                                              //  80Kbps
            cfg1 = MCP_8MHz_80kBPS_CFG1;
            cfg2 = MCP_8MHz_80kBPS_CFG2;
            cfg3 = MCP_8MHz_80kBPS_CFG3;
            break;

            case (CAN_100KBPS):                                             // 100Kbps
            cfg1 = MCP_8MHz_100kBPS_CFG1;
            cfg2 = MCP_8MHz_100kBPS_CFG2;
            cfg3 = MCP_8MHz_100kBPS_CFG3;
            break;

            case (CAN_125KBPS):                                             // 125Kbps
            cfg1 = MCP_8MHz_125kBPS_CFG1;
            cfg2 = MCP_8MHz_125kBPS_CFG2;
            cfg3 = MCP_8MHz_125kBPS_CFG3;
            break;

            case (CAN_200KBPS):                                             // 200Kbps
            cfg1 = MCP_8MHz_200kBPS_CFG1;
            cfg2 = MCP_8MHz_200kBPS_CFG2;
            cfg3 = MCP_8MHz_200kBPS_CFG3;
            break;

            case (CAN_250KBPS):                                             // 250Kbps
            cfg1 = MCP_8MHz_250kBPS_CFG1;
            cfg2 = MCP_8MHz_250kBPS_CFG2;
            cfg3 = MCP_8MHz_250kBPS_CFG3;
            break;

            case (CAN_500KBPS):                                             // 500Kbps
            cfg1 = MCP_8MHz_500kBPS_CFG1;
            cfg2 = MCP_8MHz_500kBPS_CFG2;
            cfg3 = MCP_8MHz_500kBPS_CFG3;
            break;

            case (CAN_1000KBPS):                                            //   1Mbps
            cfg1 = MCP_8MHz_1000kBPS_CFG1;
            cfg2 = MCP_8MHz_1000kBPS_CFG2;
            cfg3 = MCP_8MHz_1000kBPS_CFG3;
            break;

            default:
            set = 0;
            break;
        }
        break;

        case (MCP_16MHZ):
        switch (canSpeed)
        {
            case (CAN_5KBPS):                                               //   5Kbps
            cfg1 = MCP_16MHz_5kBPS_CFG1;
            cfg2 = MCP_16MHz_5kBPS_CFG2;
            cfg3 = MCP_16MHz_5kBPS_CFG3;
            break;

            case (CAN_10KBPS):                                              //  10Kbps
            cfg1 = MCP_16MHz_10kBPS_CFG1;
            cfg2 = MCP_16MHz_10kBPS_CFG2;
            cfg3 = MCP_16MHz_10kBPS_CFG3;
            break;

            case (CAN_20KBPS):                                              //  20Kbps
            cfg1 = MCP_16MHz_20kBPS_CFG1;
            cfg2 = MCP_16MHz_20kBPS_CFG2;
            cfg3 = MCP_16MHz_20kBPS_CFG3;
            break;

            case (CAN_33KBPS):                                              //  33.333Kbps
            cfg1 = MCP_16MHz_33k3BPS_CFG1;
            cfg2 = MCP_16MHz_33k3BPS_CFG2;
            cfg3 = MCP_16MHz_33k3BPS_CFG3;
            break;

            case (CAN_40KBPS):                                              //  40Kbps
            cfg1 = MCP_16MHz_40kBPS_CFG1;
            cfg2 = MCP_16MHz_40kBPS_CFG2;
            cfg3 = MCP_16MHz_40kBPS_CFG3;
            break;

            case (CAN_50KBPS):                                              //  50Kbps
            cfg1 = MCP_16MHz_50kBPS_CFG1;
            cfg2 = MCP_16MHz_50kBPS_CFG2;
            cfg3 = MCP_16MHz_50kBPS_CFG3;
            break;

            case (CAN_80KBPS):                                              //  80Kbps
            cfg1 = MCP_16MHz_80kBPS_CFG1;
            cfg2 = MCP_16MHz_80kBPS_CFG2;
            cfg3 = MCP_16MHz_80kBPS_CFG3;
            break;

            case (CAN_83K3BPS):                                             //  83.333Kbps
            cfg1 = MCP_16MHz_83k3BPS_CFG1;
            cfg2 = MCP_16MHz_83k3BPS_CFG2;
            cfg3 = MCP_16MHz_83k3BPS_CFG3;
            break;

            case (CAN_100KBPS):                                             // 100Kbps
            cfg1 = MCP_16MHz_100kBPS_CFG1;
            cfg2 = MCP_16MHz_100kBPS_CFG2;
            cfg3 = MCP_16MHz_100kBPS_CFG3;
            break;

            case (CAN_125KBPS):                                             // 125Kbps
            cfg1 = MCP_16MHz_125kBPS_CFG1;
            cfg2 = MCP_16MHz_125kBPS_CFG2;
            cfg3 = MCP_16MHz_125kBPS_CFG3;
            break;

            case (CAN_200KBPS):                                             // 200Kbps
            cfg1 = MCP_16MHz_200kBPS_CFG1;
            cfg2 = MCP_16MHz_200kBPS_CFG2;
            cfg3 = MCP_16MHz_200kBPS_CFG3;
            break;

            case (CAN_250KBPS):                                             // 250Kbps
            cfg1 = MCP_16MHz_250kBPS_CFG1;
            cfg2 = MCP_16MHz_250kBPS_CFG2;
            cfg3 = MCP_16MHz_250kBPS_CFG3;
            break;

            case (CAN_500KBPS):                                             // 500Kbps
            cfg1 = MCP_16MHz_500kBPS_CFG1;
            cfg2 = MCP_16MHz_500kBPS_CFG2;
            cfg3 = MCP_16MHz_500kBPS_CFG3;
            break;

            case (CAN_1000KBPS):                                            //   1Mbps
            cfg1 = MCP_16MHz_1000kBPS_CFG1;
            cfg2 = MCP_16MHz_1000kBPS_CFG2;
            cfg3 = MCP_16MHz_1000kBPS_CFG3;
            break;

            default:
            set = 0;
            break;
        }
        break;

        case (MCP_20MHZ):
        switch (canSpeed)
        {
            case (CAN_33KBPS):                                              //  33.333Kbps
            cfg1 = MCP_20MHz_33k3BPS_CFG1;
            cfg2 = MCP_20MHz_33k3BPS_CFG2;
            cfg3 = MCP_20MHz_33k3BPS_CFG3;
	    break;

            case (CAN_40KBPS):                                              //  40Kbps
            cfg1 = MCP_20MHz_40kBPS_CFG1;
            cfg2 = MCP_20MHz_40kBPS_CFG2;
            cfg3 = MCP_20MHz_40kBPS_CFG3;
            break;

            case (CAN_50KBPS):                                              //  50Kbps
            cfg1 = MCP_20MHz_50kBPS_CFG1;
            cfg2 = MCP_20MHz_50kBPS_CFG2;
            cfg3 = MCP_20MHz_50kBPS_CFG3;
            break;

            case (CAN_80KBPS):                                              //  80Kbps
            cfg1 = MCP_20MHz_80kBPS_CFG1;
            cfg2 = MCP_20MHz_80kBPS_CFG2;
            cfg3 = MCP_20MHz_80kBPS_CFG3;
            break;

            case (CAN_83K3BPS):                                             //  83.333Kbps
            cfg1 = MCP_20MHz_83k3BPS_CFG1;
            cfg2 = MCP_20MHz_83k3BPS_CFG2;
            cfg3 = MCP_20MHz_83k3BPS_CFG3;
	    break;

            case (CAN_100KBPS):                                             // 100Kbps
            cfg1 = MCP_20MHz_100kBPS_CFG1;
            cfg2 = MCP_20MHz_100kBPS_CFG2;
            cfg3 = MCP_20MHz_100kBPS_CFG3;
            break;

            case (CAN_125KBPS):                                             // 125Kbps
            cfg1 = MCP_20MHz_125kBPS_CFG1;
            cfg2 = MCP_20MHz_125kBPS_CFG2;
            cfg3 = MCP_20MHz_125kBPS_CFG3;
            break;

            case (CAN_200KBPS):                                             // 200Kbps
            cfg1 = MCP_20MHz_200kBPS_CFG1;
            cfg2 = MCP_20MHz_200kBPS_CFG2;
            cfg3 = MCP_20MHz_200kBPS_CFG3;
            break;

            case (CAN_250KBPS):                                             // 250Kbps
            cfg1 = MCP_20MHz_250kBPS_CFG1;
            cfg2 = MCP_20MHz_250kBPS_CFG2;
            cfg3 = MCP_20MHz_250kBPS_CFG3;
            break;

            case (CAN_500KBPS):                                             // 500Kbps
            cfg1 = MCP_20MHz_500kBPS_CFG1;
            cfg2 = MCP_20MHz_500kBPS_CFG2;
            cfg3 = MCP_20MHz_500kBPS_CFG3;
            break;

            case (CAN_1000KBPS):                                            //   1Mbps
            cfg1 = MCP_20MHz_1000kBPS_CFG1;
            cfg2 = MCP_20MHz_1000kBPS_CFG2;
            cfg3 = MCP_20MHz_1000kBPS_CFG3;
            break;

            default:
            set = 0;
            break;
        }
        break;

        default:
        set = 0;
        break;
    }

    if (set) {
    	spi_setRegister(MCP_CNF1, cfg1);
    	spi_setRegister(MCP_CNF2, cfg2);
    	spi_setRegister(MCP_CNF3, cfg3);
        return CAN_OK;
    }
    else {
        return CAN_FAIL;
    }
}

uint8_t MCP2515Class::setClkOut(const CAN_CLKOUT_t divisor)
{
    if (divisor == CLKOUT_DISABLE) {
	/* Turn off CLKEN */
    	spi_modifyRegister(MCP_CANCTRL, CANCTRL_CLKEN, 0x00);

	/* Turn on CLKOUT for SOF */
    	spi_modifyRegister(MCP_CNF3, CNF3_SOF, CNF3_SOF);
        return CAN_OK;
    }

    /* Set the prescaler (CLKPRE) */
    spi_modifyRegister(MCP_CANCTRL, CANCTRL_CLKPRE, divisor);

    /* Turn on CLKEN */
    spi_modifyRegister(MCP_CANCTRL, CANCTRL_CLKEN, CANCTRL_CLKEN);

    /* Turn off CLKOUT for SOF */
    spi_modifyRegister(MCP_CNF3, CNF3_SOF, 0x00);
    return CAN_OK;
}

void MCP2515Class::prepareId(uint8_t *buffer, const bool ext, const uint32_t id)
{
    uint16_t canid = (uint16_t)(id & 0x0FFFF);

    if (ext) {
        buffer[MCP_EID0] = (uint8_t) (canid & 0xFF);
        buffer[MCP_EID8] = (uint8_t) (canid >> 8);
        canid = (uint16_t)(id >> 16);
        buffer[MCP_SIDL] = (uint8_t) (canid & 0x03);
        buffer[MCP_SIDL] += (uint8_t) ((canid & 0x1C) << 3);
        buffer[MCP_SIDL] |= TXB_EXIDE_MASK;
        buffer[MCP_SIDH] = (uint8_t) (canid >> 5);
    } else {
        buffer[MCP_SIDH] = (uint8_t) (canid >> 3);
        buffer[MCP_SIDL] = (uint8_t) ((canid & 0x07 ) << 5);
        buffer[MCP_EID0] = 0;
        buffer[MCP_EID8] = 0;
    }
}

uint8_t MCP2515Class::setFilterMask(const MASK_t mask, const bool ext, const uint32_t ulData)
{
    uint8_t res = setMode(MODE_CONFIG);
    if (res != CAN_OK) {
        return res;
    }

    uint8_t tbufdata[4];
    prepareId(tbufdata, ext, ulData);

    REGISTER_t reg;
    switch (mask) {
        case MASK0: reg = MCP_RXM0SIDH; break;
        case MASK1: reg = MCP_RXM1SIDH; break;
        default:
            return CAN_FAIL;
    }

    spi_setRegisters(reg, tbufdata, 4);

    return CAN_OK;
}

uint8_t MCP2515Class::setFilter(const RXF_t num, const bool ext, const uint32_t ulData)
{
    uint8_t res = setMode(MODE_CONFIG);
    if (res != CAN_OK) {
        return res;
    }

    REGISTER_t reg;

    switch (num) {
        case RXF0: reg = MCP_RXF0SIDH; break;
        case RXF1: reg = MCP_RXF1SIDH; break;
        case RXF2: reg = MCP_RXF2SIDH; break;
        case RXF3: reg = MCP_RXF3SIDH; break;
        case RXF4: reg = MCP_RXF4SIDH; break;
        case RXF5: reg = MCP_RXF5SIDH; break;
        default:
            return CAN_FAIL;
    }

    uint8_t tbufdata[4];
    prepareId(tbufdata, ext, ulData);
    spi_setRegisters(reg, tbufdata, 4);

    return CAN_OK;
}

uint8_t MCP2515Class::sendMessage(const TXBn_t txbn, can_message_t *tx_message)
{
    tx_message->dlc_non_comp = 0;
    if (tx_message->data_length_code > CAN_MAX_DLC) {
        tx_message->dlc_non_comp = 1;
        return CAN_FAILTX_DLC;
    }

    const auto txbuf = &TXB_ptr[txbn];

    uint8_t data[13];

    bool ext = tx_message->extd;
    bool rtr = tx_message->rtr;
    uint32_t id = (tx_message->identifier & (ext ? CAN_EFF_MASK : CAN_SFF_MASK));

    prepareId(data, ext, id);

    data[MCP_DLC] = rtr ? (tx_message->data_length_code | RTR_MASK) : tx_message->data_length_code;

    memcpy(&data[MCP_DATA], tx_message->data, tx_message->data_length_code);

    spi_setRegisters(txbuf->SIDH, data, 5 + tx_message->data_length_code);

    spi_modifyRegister(txbuf->CTRL, TXB_TXREQ, TXB_TXREQ);

    uint8_t ctrl = spi_readRegister(txbuf->CTRL);
    if ((ctrl & (TXB_ABTF | TXB_MLOA | TXB_TXERR)) != 0) {
        return CAN_FAILTX | (ctrl & (TXB_ABTF | TXB_MLOA | TXB_TXERR));
    }
    return CAN_OK;
}

uint8_t MCP2515Class::sendMsgBuf(can_message_t *tx_message)
{
    tx_message->dlc_non_comp = 0;
    if (tx_message->data_length_code > CAN_MAX_DLC) {
        tx_message->dlc_non_comp = 1;
        return CAN_FAILTX;
    }

    TXBn_t txBuffers[N_TXBUFFERS] = {TXB0, TXB1, TXB2};

    uint32_t uiTimeOut, temp;
    temp = micros();
    // 24 * 4 microseconds typical
    do {
        for (int i=0; i<N_TXBUFFERS; i++) {
            const auto txbuf = &TXB_ptr[txBuffers[i]];
            if ( (spi_readRegister(txbuf->CTRL) & TXB_TXREQ) == 0 ) {
                return sendMessage(txBuffers[i], tx_message);
            }
        }
        uiTimeOut = micros() - temp;
    } while ((uiTimeOut < MCP_TIMEOUTVALUE));

    if(uiTimeOut >= MCP_TIMEOUTVALUE)
    {
        return CAN_GETTXBFTIMEOUT;    // get tx buff time out
    }

    return CAN_ALLTXBUSY;
}

uint8_t MCP2515Class::readMessage(const RXBn_t rxbn, can_message_t *rx_message)
{
    uint8_t rc = CAN_OK;
    uint8_t tbufdata[5];
    const auto rxb = &RXB_ptr[rxbn];

    rx_message->identifier = 0;
    rx_message->data_length_code = 0;
    rx_message->flags = 0;
    memset(rx_message->data, 0, sizeof(rx_message->data));

    spi_readRegisters(rxb->SIDH, tbufdata, 5);  // read 4 bytes of id and dlc

    uint32_t id = (tbufdata[MCP_SIDH]<<3) + (tbufdata[MCP_SIDL]>>5);

    if ( (tbufdata[MCP_SIDL] & TXB_EXIDE_MASK) ==  TXB_EXIDE_MASK ) {
        // extended id
        id = (id<<2) + (tbufdata[MCP_SIDL] & 0x03);
        id = (id<<8) + tbufdata[MCP_EID8];
        id = (id<<8) + tbufdata[MCP_EID0];
        rx_message->extd = 1;
    }

    uint8_t dlc = (tbufdata[MCP_DLC] & DLC_MASK);
    if (dlc > CAN_MAX_DLC) {
        rx_message->dlc_non_comp = 1;
        dlc = CAN_MAX_DLC;
        rc = CAN_FAILRX_DLC;
    }
    if (spi_readRegister(rxb->CTRL) & RXBnCTRL_RTR) {
        rx_message->rtr = 1;
    }

    rx_message->identifier = id;
    rx_message->data_length_code = dlc;

    spi_readRegisters(rxb->DATA, rx_message->data, dlc);

    spi_modifyRegister(MCP_CANINTF, rxb->CANINTF_RXnIF, 0);

    return rc;
}

uint8_t MCP2515Class::readMsgBuf(can_message_t *rx_message)
{
    uint8_t rc;
    uint8_t stat = spi_getStatus();

    if ( stat & STAT_RX0IF ) {
        rc = readMessage(RXB0, rx_message);
    } else if ( stat & STAT_RX1IF ) {
        rc = readMessage(RXB1, rx_message);
    } else {
        rc = CAN_NOMSG;
    }

    return rc;
}

uint8_t MCP2515Class::checkReceive(void)
{
    if ( spi_getStatus() & STAT_RXIF_MASK ) {
        return CAN_MSGAVAIL;
    } else {
        return CAN_NOMSG;
    }
}

uint8_t MCP2515Class::checkError(void)
{
    if ( getErrorFlags() & EFLG_ERRORMASK ) {
        return CAN_CTRLERROR;
    } else {
        return CAN_OK;
    }
}

uint8_t MCP2515Class::getErrorFlags(void)
{
    return spi_readRegister(MCP_EFLG);
}

/*********************************************************************************************************
** Function name:           errorCountTX
** Descriptions:            Returns TEC register value
*********************************************************************************************************/
uint8_t MCP2515Class::errorCountRX(void)
{
    return spi_readRegister(MCP_REC);
}

/*********************************************************************************************************
** Function name:           errorCountTX
** Descriptions:            Returns TEC register value
*********************************************************************************************************/
uint8_t MCP2515Class::errorCountTX(void)
{
    return spi_readRegister(MCP_TEC);
}

void MCP2515Class::clearRXnOVRFlags(void)
{
	spi_modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
}

uint8_t MCP2515Class::getInterrupts(void)
{
    return spi_readRegister(MCP_CANINTF);
}

void MCP2515Class::clearInterrupts(void)
{
	spi_setRegister(MCP_CANINTF, 0);
}

uint8_t MCP2515Class::getInterruptMask(void)
{
    return spi_readRegister(MCP_CANINTE);
}

void MCP2515Class::clearTXInterrupts(void)
{
	spi_modifyRegister(MCP_CANINTF, (CANINTF_TX0IF | CANINTF_TX1IF | CANINTF_TX2IF), 0);
}

void MCP2515Class::clearRXnOVR(void)
{
	if (getErrorFlags() != 0) {
		clearRXnOVRFlags();
		clearInterrupts();
		//modifyRegister(MCP_CANINTF, CANINTF_ERRIF, 0);
	}
}

void MCP2515Class::clearMERR()
{
	//modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
	//clearInterrupts();
	spi_modifyRegister(MCP_CANINTF, CANINTF_MERRF, 0);
}

void MCP2515Class::clearERRIF()
{
    //modifyRegister(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0);
    //clearInterrupts();
	spi_modifyRegister(MCP_CANINTF, CANINTF_ERRIF, 0);
}
