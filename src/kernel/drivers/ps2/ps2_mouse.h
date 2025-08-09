#pragma once

#include "ps2.h"

// TODO: Implement scrolling and buttons 4 and 5.

typedef enum
{
    PS2_PACKET_BUTTON_LEFT = (1 << 0),
    PS2_PACKET_BUTTON_RIGHT = (1 << 1),
    PS2_PACKET_BUTTON_MIDDLE = (1 << 2),
    PS2_PACKET_ALWAYS_ONE = (1 << 3),
    PS2_PACKET_X_SIGN = (1 << 4),
    PS2_PACKET_Y_SIGN = (1 << 5),
    PS2_PACKET_X_OVERFLOW = (1 << 6),
    PS2_PACKET_Y_OVERFLOW = (1 << 7),
} ps2_mouse_packet_flags_t;

typedef struct ps2_mouse_packet
{
    ps2_mouse_packet_flags_t flags;
    int16_t deltaX;
    int16_t deltaY;
} ps2_mouse_packet_t;

typedef struct ps2_mouse_irq_context
{
    uint8_t index;
    ps2_mouse_packet_t packet;
} ps2_mouse_irq_context_t;

uint64_t ps2_mouse_init(ps2_device_info_t* info);
