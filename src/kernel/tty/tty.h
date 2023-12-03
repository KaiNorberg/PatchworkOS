#pragma once

#include <stdint.h>

#include "kernel/gop/gop.h"
#include "kernel/psf/psf.h"

typedef enum
{
    TTY_MESSAGE_OK,
    TTY_MESSAGE_ER
} TTY_MESSAGE;

void tty_init(Framebuffer* screenbuffer, PSFFont* screenFont);
void tty_put(uint8_t chr);
void tty_print(const char* string);  
void tty_printi(uint64_t integer);
void tty_printx(uint64_t hex);

void tty_clear();

void tty_set_scale(uint8_t scale);
void tty_set_foreground(Pixel color);
void tty_set_background(Pixel color);

void tty_set_cursor_pos(uint64_t x, uint64_t y);

void tty_start_message(const char* message);
void tty_end_message(uint64_t status);