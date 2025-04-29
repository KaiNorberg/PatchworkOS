#ifndef _DWM_EVENT_H
#define _DWM_EVENT_H 1

#include "surface.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t event_type_t;

// Dwm events, send from the dwm.
#define EVENT_SCREEN_INFO 0
#define EVENT_INIT 1
#define EVENT_REDRAW 2

// Library events, defined by libdwm.
#define LEVENT_BASE (1 << 14)
#define LEVENT_FREE (LEVENT_BASE + 1)

// User events, defined by individual programs
#define UEVENT_BASE (1 << 15)

typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

typedef struct event
{
    event_type_t type;
    surface_id_t target;
    uint8_t data[64];
} event_t;

#if defined(__cplusplus)
}
#endif

#endif
