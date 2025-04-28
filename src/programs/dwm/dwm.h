#pragma once

#include "surface.h"

#include <sys/io.h>
#include <sys/list.h>

void dwm_init(void);

void dwm_deinit(void);

uint64_t dwm_attach_to_wall(surface_t* surface);

void dwm_set_redraw_needed(void);

void dwm_loop(void);
