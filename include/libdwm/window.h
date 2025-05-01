#ifndef DWM_WIN_H
#define DWM_WIN_H 1

#include "display.h"
#include "pixel.h"
#include "procedure.h"
#include "surface.h"

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

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure);

void window_free(window_t* win);

void window_rect(window_t* win, rect_t* rect);

void window_content_rect(window_t* win, rect_t* rect);

uint64_t window_dispatch(window_t* win, event_t* event);

#if defined(__cplusplus)
}
#endif

#endif
