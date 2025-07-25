#pragma once

#include <stdint.h>

#include <boot/boot_info.h>

#include "config.h"
#include "log/glyphs.h"
#include "utils/ring.h"

#define SCREEN_WRAP_INDENT 4

typedef struct
{
    uint64_t x;
    uint64_t y;
} screen_pos_t;

#define SCREEN_LINE_MAX_LENGTH (256)
#define SCREEN_LINE_STRIDE (SCREEN_LINE_MAX_LENGTH * GLYPH_WIDTH)

typedef struct
{
    uint64_t length; //!< The distance from the start of the line to the furthest away char, in chars.
    uint32_t pixels[GLYPH_HEIGHT * SCREEN_LINE_STRIDE];
} screen_line_t;

typedef struct
{
    uint64_t width;  //!< The width of the buffer in chars.
    uint64_t height; //!< The height of the buffer in chars.
    uint64_t firstLineIndex;
    screen_pos_t invalidStart;
    screen_pos_t invalidEnd;
    screen_line_t lines[CONFIG_SCREEN_MAX_LINES];
} screen_buffer_t;

typedef struct
{
    bool initialized;
    boot_gop_t gop;
    screen_pos_t cursor;
    screen_buffer_t buffer;
} screen_t;

void screen_init(screen_t* screen, const boot_gop_t* gop);

void screen_enable(screen_t* screen, const ring_t* ring);

void screen_disable(screen_t* screen);

void screen_write(screen_t* screen, const char* string, uint64_t length);
