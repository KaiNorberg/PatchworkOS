#pragma once

#include "defs.h"

typedef struct ps2_mouse_packet
{
    uint8_t flags;
    uint8_t deltaX;
    uint8_t deltaY;
} ps2_mouse_packet_t;

#define PS2_PACKET_BUTTON_LEFT (1 << 0)
#define PS2_PACKET_BUTTON_RIGHT (1 << 1)
#define PS2_PACKET_BUTTON_MIDDLE (1 << 2)
#define PS2_PACKET_ALWAYS_ONE (1 << 3)
#define PS2_PACKET_X_SIGN (1 << 4)
#define PS2_PACKET_Y_SIGN (1 << 5)
#define PS2_PACKET_X_OVERFLOW (1 << 6)
#define PS2_PACKET_Y_OVERFLOW (1 << 7)

void ps2_mouse_init(void);
