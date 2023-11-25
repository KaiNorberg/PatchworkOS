#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"

void tty_init(Framebuffer* screenbuffer, PSFFont* screenFont);
void tty_put(uint8_t chr);
void tty_print(const char* string);  