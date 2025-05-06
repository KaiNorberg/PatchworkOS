#pragma once

#include <stdint.h>

#include "surface.h"

#define TILE_SIZE 32

typedef struct
{
    bool* map;
    uint64_t* indices;
    uint64_t columns;
    uint64_t rows;
    uint64_t totalAmount;
    uint64_t invalidAmount;
} tiles_t;

void screen_init(void);

void screen_deinit(void);

void screen_transfer(surface_t* surface, const rect_t* rect);

void screen_transfer_blend(surface_t* surface, const rect_t* rect);

void screen_swap(void);

uint64_t screen_width(void);

uint64_t screen_height(void);

void screen_rect(rect_t* rect);
