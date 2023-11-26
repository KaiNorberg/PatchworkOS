#pragma once

#include <stdint.h>

#include "kernel/gop/gop.h"
#include "kernel/psf/psf.h"

void tty_init(Framebuffer* screenbuffer, PSFFont* screenFont);
void tty_put(uint8_t chr);
void tty_print(const char* string);  
void tty_printi(uint64_t integer);