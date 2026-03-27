#pragma once

#include <patchwork/event.h>
#include <libstd/kbd.h>

keycode_t kbd_translate(keycode_t code);

char kbd_ascii(keycode_t code, kbd_mods_t mods);
