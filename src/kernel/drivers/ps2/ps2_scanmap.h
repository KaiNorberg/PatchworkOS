#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/kbd.h>

#define PS2_EXTEND_BYTE 0xE0

#define PS2_BYTE_RELEASE_FLAG (1 << 7)

#define PS2_SCAN_CODE_SET 1

typedef struct
{
    uint8_t scancode;
    bool isExtendCode;
    bool isReleased;
} ps2_scancode_t;

void ps2_scancode_from_byte(ps2_scancode_t* scancode, uint8_t byte);

keycode_t ps2_scancode_to_keycode(ps2_scancode_t* scancode, bool isExtended);
