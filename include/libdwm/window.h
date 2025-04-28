#ifndef _DWM_WIN_H
#define _DWM_WIN_H 1

#include "display.h"
#include "surface.h"
#include "pixel.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct window window_t;

typedef enum win_flags
{
    WINDOW_NONE = 0,
    WINDOW_DECO = (1 << 0)
} window_flags_t;

typedef uint64_t (*procedure_t)(window_t*, const event_t*);

window_t* window_new(display_t* disp, window_t* parent, const char* name, const rect_t* rect, surface_id_t id, surface_type_t type, window_flags_t flags, procedure_t procedure);

void window_free(window_t* win);

void window_get_rect(window_t* win, rect_t* rect);

void window_draw_rect(window_t* win, const rect_t* rect, pixel_t pixel);

#if defined(__cplusplus)
}
#endif

#endif
