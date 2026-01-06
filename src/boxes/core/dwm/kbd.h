#pragma once

#include <sys/kbd.h>
#include <libpatchwork/event.h>

keycode_t kbd_translate(keycode_t code);

char kbd_ascii(keycode_t code, kbd_mods_t mods);
