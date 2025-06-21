#pragma once

#include <stdint.h>

#include <boot/boot_info.h>

#include "utils/ring.h"

#define SCREEN_WRAP_INDENT 4

typedef struct
{
    uint64_t x;
    uint64_t y;
} screen_pos_t;

typedef struct
{
    uint64_t length;
    uint32_t pixels[];
} screen_line_t;

typedef struct
{
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    uint64_t lineSize;
    uint64_t firstLineIndex;
    screen_pos_t invalidStart;
    screen_pos_t invalidEnd;
    screen_line_t** lines;
    uint8_t* storage;
} screen_buffer_t;

typedef struct
{
    bool initialized;
    gop_buffer_t framebuffer;
    screen_pos_t cursor;
    screen_buffer_t buffer;
} screen_t;

uint64_t screen_init(screen_t* screen, const gop_buffer_t* framebuffer);

void screen_enable(screen_t* screen, const ring_t* ring);

void screen_disable(screen_t* screen);

void screen_write(screen_t* screen, const char* string, uint64_t length);