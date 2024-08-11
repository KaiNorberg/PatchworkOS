#pragma once

#include <stdint.h>
#include <sys/kbd.h>

keycode_t ps2_scancode_to_keycode(uint64_t scancode);
