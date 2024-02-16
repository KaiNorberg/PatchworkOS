#pragma once

#include <stdint.h>

#include "gop/gop.h"

#include <common/boot_info/boot_info.h>

#define TTY_CHAR_HEIGHT 16
#define TTY_CHAR_WIDTH 8

typedef struct __attribute__((packed))
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} Pixel;

typedef enum
{
    TTY_MESSAGE_OK,
    TTY_MESSAGE_ER
} TTY_MESSAGE;

void tty_init(GopBuffer* screenbuffer, PsfFont* screenFont);

void tty_acquire();

void tty_release();

void tty_scroll(uint64_t distance);

void tty_put(uint8_t chr);
void tty_print(const char* string);  
void tty_printi(uint64_t integer);
void tty_printx(uint64_t hex);

void tty_clear();

void tty_set_scale(uint8_t newScale);
void tty_set_foreground(Pixel color);
void tty_set_background(Pixel color);

void tty_set_cursor_pos(uint64_t x, uint64_t y);
Point tty_get_cursor_pos();

uint32_t tty_get_screen_width();
uint32_t tty_get_screen_height();

void tty_start_message(const char* message);
void tty_assert(uint8_t expression, const char* message);
void tty_end_message(uint64_t status);