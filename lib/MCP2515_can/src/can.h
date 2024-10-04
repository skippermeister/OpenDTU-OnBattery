// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CAN_H_
#define CAN_H_

#include <Arduino.h>
#include <inttypes.h>

/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG 0x80000000UL /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000UL /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000UL /* error message frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFUL /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFUL /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFUL /* omit EFF, RTR, ERR flags */

/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28 : CAN identifier (11/29 bit)
 * bit 29   : error message frame flag (0 = data frame, 1 = error message)
 * bit 30   : remote transmission request flag (1 = rtr frame)
 * bit 31   : frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */
#define CAN_SFF_ID_BITS     11
#define CAN_EFF_ID_BITS     29

/* CAN payload length and DLC definitions according to ISO 11898-1 */
#define CAN_MAX_DLC 8

struct can_message_t {
    union {
        struct {
        unsigned extd:1;
        unsigned rtr:1;
        unsigned ss:1;
        unsigned self:1;
        unsigned dlc_non_comp:1;
        unsigned reserved:27;
        };
        uint32_t flags;
    };
    uint32_t identifier;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
    uint8_t data_length_code; /* frame payload length in byte (0 .. CAN_MAX_DLC) */
    uint8_t data[CAN_MAX_DLC] __attribute__((aligned(8)));
};

#endif /* CAN_H_ */
