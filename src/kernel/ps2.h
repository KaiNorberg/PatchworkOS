#pragma once

#include "defs.h"
#include "sysfs.h"

#define PS2_KEY_BUFFER_LENGTH 32

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
#define PS2_CMD_KDB_ENABLE 0xAE
#define PS2_CMD_SCANCODE_SET 0xF0

#define PS2_STATUS_OUT_FULL (1 << 0)
#define PS2_STATUS_IN_FULL (1 << 1)
#define PS2_STATUS_TIME_OUT (1 << 6)

#define PS2_CFG_KBD_IRQ (1 << 0)

#define SCANCODE_RELEASED (1 << 7)

void ps2_init();