#pragma once

#include "surface.h"

#include <libpatchwork/event.h>
#include <sys/io.h>
#include <sys/list.h>

/**
 * @brief Desktop Window Manager.
 * @defgroup programs_dwm Desktop Window Manager
 * @ingroup programs
 *
 * The Desktop Window Manager (DWM) is the window manager for PatchworkOS. It is responsible for managing windows,
 * panels, cursors, and other surfaces, as well as handling input events from the keyboard and mouse.
 *
 * @{
 */

typedef struct
{
    pollfd_t data;
    pollfd_t kbd;
    pollfd_t mouse;
    pollfd_t clients[];
} poll_ctx_t;

void dwm_init(void);

void dwm_deinit(void);

void dwm_report_produce(surface_t* surface, client_t* client, report_flags_t flags);

surface_t* dwm_surface_find(surface_id_t id);

uint64_t dwm_attach(surface_t* surface);

void dwm_detach(surface_t* surface);

void dwm_focus_set(surface_t* surface);

void dwm_loop(void);

/** @} */
