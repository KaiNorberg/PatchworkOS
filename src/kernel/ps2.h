#pragma once

#include "defs.h"
#include "sysfs.h"

#define PS2_DATA_PORT 0x60
#define PS2_CMD_PORT 0x64
#define PS2_STATUS_PORT 0x64

#define PS2_CMD_READ_CFG 0x20
#define PS2_CMD_WRITE_CFG 0x60
#define PS2_CMD_DISABLE_2 0xA7
#define PS2_CMD_ENABLE_2 0xA8
#define PS2_CMD_TEST_2 0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_1 0xAB
#define PS2_CMD_DISABLE_1 0xAD
#define PS2_CMD_ENABLE_1 0xAE
#define PS2_CMD_SCANCODE_SET 0xF0

#define SCANCODE_RELEASED (1 << 7)

#define ENTER 0x1C
#define BACKSPACE 0x0E
#define CONTROL 0x1D
#define LEFT_SHIFT 0x2A
#define ARROW_UP 0x48
#define ARROW_DOWN 0x50
#define ARROW_LEFT 0x4B
#define ARROW_RIGHT 0x4D
#define PAGE_UP 0x49
#define PAGE_DOWN 0x51
#define CAPS_LOCK 0x3A

typedef struct
{
    Resource base;
} Ps2Keyboard;

void ps2_init();