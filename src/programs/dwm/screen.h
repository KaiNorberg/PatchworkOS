#pragma once

#include <stdint.h>

#include "surface.h"

typedef struct
{
    bool invalid;
    int64_t start;
    int64_t end;
} scanline_t;

void screen_init(void);

void screen_deinit(void);

void screen_transfer(surface_t* surface, const rect_t* rect);

void screen_transfer_blend(surface_t* surface, const rect_t* rect);

void screen_swap(void);

uint64_t screen_width(void);

uint64_t screen_height(void);

void screen_rect(rect_t* rect);

uint64_t screen_acquire(void);

uint64_t screen_release(void);
