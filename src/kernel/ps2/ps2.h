#pragma once

#include "defs.h"

#define PS2_BUFFER_LENGTH 32

#define PS2_DATA_REPORTING_ENABLE 0xF4

#define PS2_PORT_DATA 0x60
#define PS2_PORT_CMD 0x64
#define PS2_PORT_STATUS 0x64

#define PS2_CMD_CFG_READ 0x20
#define PS2_CMD_CFG_WRITE 0x60
#define PS2_CMD_AUX_DISABLE 0xA7
#define PS2_CMD_AUX_ENABLE 0xA8
#define PS2_CMD_AUX_TEST 0xA9
#define PS2_CMD_CONTROLLER_TEST 0xAA
#define PS2_CMD_KBD_TEST 0xAB
#define PS2_CMD_KBD_DISABLE 0xAD
#define PS2_CMD_KBD_ENABLE 0xAE
#define PS2_CMD_AUX_WRITE 0xD4
#define PS2_CMD_SCANCODE_SET 0xF0

#define PS2_STATUS_OUT_FULL (1 << 0)
#define PS2_STATUS_IN_FULL (1 << 1)
#define PS2_STATUS_TIME_OUT (1 << 6)

#define PS2_CFG_KBD_IRQ (1 << 0)
#define PS2_CFG_AUX_IRQ (1 << 1)

#define PS2_ENABLE_DATA_REPORTING 0xF4
#define PS2_SET_DEFAULTS 0xF6

#define PS2_ACK 0xFA

#define SCANCODE_RELEASED (1 << 7)

void ps2_init(void);

uint8_t ps2_read(void);

void ps2_write(uint8_t data);

void ps2_wait(void);

void ps2_cmd(uint8_t command);
