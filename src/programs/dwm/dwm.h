#pragma once

#include "surface.h"

#include <libdwm/event.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct
{
    pollfd_t data;
    pollfd_t kbd;
    pollfd_t mouse;
    pollfd_t clients[];
} poll_ctx_t;

void dwm_init(void);

void dwm_deinit(void);

psf_t* dwm_default_font(void);

uint64_t dwm_attach(surface_t* surface);

void dwm_detach(surface_t* surface);

void dwm_focus_set(surface_t* surface);

void dwm_loop(void);
