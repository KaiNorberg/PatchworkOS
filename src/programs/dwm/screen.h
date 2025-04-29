#pragma once

#include <stdint.h>

#include "surface.h"

void screen_init(void);

void screen_deinit(void);

void screen_transfer(surface_t* surface, const rect_t* rect);

void screen_swap(void);

uint64_t screen_width(void);

uint64_t screen_height(void);

void screen_rect(rect_t* rect);
