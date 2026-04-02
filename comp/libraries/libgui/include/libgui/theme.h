#pragma once

#include <libgfx/gfx.h>

/**
 * @brief Theme definitions
 * @defgroup comp_libgui_theme Theme
 * @ingroup comp
 * 
 * The default theme is inspired by the Nord color palette.
 * 
 * @todo Dont hardcode colors and stuff, add in a theming system.
 * 
 * @see [https://www.nordtheme.com/docs/colors-and-palettes](Nord Colors and Palettes)
 * 
 * @{
 */

#define GUI_THEME_BACK_0 GFX_PIXEL(0xFF, 0x2E, 0x34, 0x40)
#define GUI_THEME_BACK_1 GFX_PIXEL(0xFF, 0x3B, 0x42, 0x52)
#define GUI_THEME_BACK_2 GFX_PIXEL(0xFF, 0x43, 0x4C, 0x5E)
#define GUI_THEME_BACK_3 GFX_PIXEL(0xFF, 0x4C, 0x56, 0x6A)

#define GUI_THEME_FORE_0 GFX_PIXEL(0xFF, 0xD8, 0xDE, 0xE9)
#define GUI_THEME_FORE_1 GFX_PIXEL(0xFF, 0xE5, 0xE9, 0xF0)
#define GUI_THEME_FORE_2 GFX_PIXEL(0xFF, 0xEC, 0xEF, 0xF4)

#define GUI_THEME_ACCENT_0 GFX_PIXEL(0xFF, 0x8F, 0xBC, 0xBB)
#define GUI_THEME_ACCENT_1 GFX_PIXEL(0xFF, 0x88, 0xC0, 0xD0)
#define GUI_THEME_ACCENT_2 GFX_PIXEL(0xFF, 0x81, 0xA1, 0xC1)
#define GUI_THEME_ACCENT_3 GFX_PIXEL(0xFF, 0x5E, 0x81, 0xAC)

#define GUI_THEME_RED    GFX_PIXEL(0xFF, 0xBF, 0x61, 0x6A)
#define GUI_THEME_ORANGE GFX_PIXEL(0xFF, 0xD0, 0x87, 0x70)
#define GUI_THEME_YELLOW GFX_PIXEL(0xFF, 0xEB, 0xCB, 0x8B)
#define GUI_THEME_GREEN  GFX_PIXEL(0xFF, 0xA3, 0xBE, 0x8C)
#define GUI_THEME_PURPLE GFX_PIXEL(0xFF, 0xB4, 0x8E, 0xAD)

#define GUI_THEME_SMALL_RADIUS 4
#define GUI_THEME_MEDIUM_RADIUS 8
#define GUI_THEME_LARGE_RADIUS  12

#define GUI_THEME_PADDING_SMALL  4
#define GUI_THEME_PADDING_MEDIUM 8
#define GUI_THEME_PADDING_LARGE  12

#define GUI_THEME_BORDER_WIDTH 1

/** @} */