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

typedef struct win_theme
{
    uint8_t edgeWidth;
    uint8_t rimWidth;
    uint8_t ridgeWidth;
    pixel_t highlight;
    pixel_t shadow;
    pixel_t bright;
    pixel_t dark;
    pixel_t background;
    pixel_t selected;
    pixel_t selectedHighlight;
    pixel_t unSelected;
    pixel_t unSelectedHighlight;
    uint8_t topbarHeight;
    uint8_t paddingWidth;
} window_theme_t;
extern window_theme_t windowTheme;

typedef uint64_t (*procedure_t)(window_t*, const event_t*);

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags, procedure_t procedure);

void window_free(window_t* win);

void window_get_rect(window_t* win, rect_t* rect);

void window_get_local_rect(window_t* win, rect_t* rect);

void window_draw_rect(window_t* win, const rect_t* rect, pixel_t pixel);

void window_draw_edge(window_t* win, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void window_draw_gradient(window_t* win, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise);

#if defined(__cplusplus)
}
#endif

#endif
