#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/kbd.h>

keycode_t ps2_scancode_to_keycode(bool extended, uint8_t scancode);
