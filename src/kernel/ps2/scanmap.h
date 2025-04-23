#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/kbd.h>

keycode_t ps2_scancode_to_keycode(bool extended, uint8_t scancode);
