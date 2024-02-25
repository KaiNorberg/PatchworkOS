#pragma once

#include <stdint.h>

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

void tty_init(GopBuffer* gopBuffer, PsfFont* screenFont);

void tty_set_scale(uint8_t value);
void tty_set_foreground(Pixel value);
void tty_set_background(Pixel value);

void tty_set_row(uint32_t value);
uint32_t tty_get_row();

void tty_set_column(uint32_t value);
uint32_t tty_get_column();

uint32_t tty_row_amount();
uint32_t tty_column_amount();

void tty_acquire();
void tty_release();

void tty_put(uint8_t chr);
void tty_print(const char* string);
void tty_printi(uint64_t integer);
void tty_printx(uint64_t hex);
void tty_printm(const char* string, uint64_t length);

void tty_clear();

void tty_start_message(const char* message);
void tty_assert(uint8_t expression, const char* message);
void tty_end_message(uint64_t status);