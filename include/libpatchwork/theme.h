#ifndef PATCHWORK_THEME_H
#define PATCHWORK_THEME_H 1

#include "pixel.h"
#include "window.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define COLOR_INVALID ((pixel_t)0xFFFF00FF)

typedef struct theme_override_buffer theme_override_buffer_t;

typedef struct
{
    theme_override_buffer_t* buffer; // Initalized to NULL
} theme_override_t;

typedef enum
{
    COLOR_SET_BUTTON,  // Buttons and similar elements.
    COLOR_SET_VIEW,    // View/Input elements, for example labels.
    COLOR_SET_ELEMENT, // Other elements.
    COLOR_SET_PANEL,   // Panels, for example taskbars.
    COLOR_SET_DECO,    // Window decorations.
    COLOR_SET_AMOUNT,
} theme_color_set_t;

typedef enum
{
    COLOR_ROLE_BACKGROUND_NORMAL,
    COLOR_ROLE_BACKGROUND_SELECTED_START,
    COLOR_ROLE_BACKGROUND_SELECTED_END,
    COLOR_ROLE_BACKGROUND_UNSELECTED_START,
    COLOR_ROLE_BACKGROUND_UNSELECTED_END,
    COLOR_ROLE_FOREGROUND_NORMAL,
    COLOR_ROLE_FOREGROUND_INACTIVE,
    COLOR_ROLE_FOREGROUND_LINK,
    COLOR_ROLE_FOREGROUND_SELECTED,
    COLOR_ROLE_BEZEL,
    COLOR_ROLE_HIGHLIGHT,
    COLOR_ROLE_SHADOW,
    COLOR_ROLE_AMOUNT
} theme_color_role_t;

pixel_t theme_get_color(theme_color_set_t set, theme_color_role_t role, theme_override_t* override);

typedef enum
{
    STRING_WALLPAPER,
    STRING_FONTS_DIR,
    STRING_CURSOR_ARROW,
    STRING_DEFAULT_FONT,
    STRING_ICON_CLOSE,
    STRING_AMOUNT,
} theme_string_t;

const char* theme_get_string(theme_string_t name, theme_override_t* override);

typedef enum
{
    INT_FRAME_SIZE,
    INT_BEZEL_SIZE,
    INT_TITLEBAR_SIZE,
    INT_PANEL_SIZE,
    INT_BIG_PADDING,
    INT_SMALL_PADDING,
    INT_SEPERATOR_SIZE,
    INT_AMOUNT,
} theme_int_t;

int64_t theme_get_int(theme_int_t name, theme_override_t* override);

void theme_override_init(theme_override_t* override);

void theme_override_deinit(theme_override_t* override);

uint64_t theme_override_set_color(theme_override_t* override, theme_color_set_t set, theme_color_role_t role,
    pixel_t color);

uint64_t theme_override_set_string(theme_override_t* override, theme_string_t name, const char* string);

uint64_t theme_override_set_int(theme_override_t* override, theme_int_t name, int64_t integer);

#if defined(__cplusplus)
}
#endif

#endif
