#ifndef PATCHWORK_WIN_H
#define PATCHWORK_WIN_H 1

#include "display.h"
#include "pixel.h"
#include "procedure.h"
#include "surface.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define WINDOW_CLIENT_ELEM_ID (UINT64_MAX)
#define WINDOW_DECO_ELEM_ID (UINT64_MAX - 1)

typedef struct window window_t;

typedef enum win_flags
{
    WINDOW_NONE = 0,
    WINDOW_DECO = (1 << 0),
    WINDOW_RESIZABLE = (1 << 1),
    WINDOW_NO_CONTROLS = (1 << 2)
} window_flags_t;

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure, void* private);

void window_free(window_t* win);

void window_rect_get(window_t* win, rect_t* rect);

void window_content_rect(window_t* win, rect_t* rect);

display_t* window_display_get(window_t* win);

surface_id_t window_id_get(window_t* win);

surface_type_t window_type_get(window_t* win);

element_t* window_client_element_get(window_t* win);

uint64_t window_move(window_t* win, const rect_t* rect);

uint64_t window_timer_set(window_t* win, timer_flags_t flags, clock_t timeout);

void window_invalidate(window_t* win, const rect_t* rect);

void window_invalidate_flush(window_t* win);

uint64_t window_dispatch(window_t* win, const event_t* event);

void window_focus_set(window_t* win);

#if defined(__cplusplus)
}
#endif

#endif
