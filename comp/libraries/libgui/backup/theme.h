#pragma once

#include <libgfx/gfx.h>

/**
 * @brief Theme definitions
 * @defgroup comp_libgui_theme Theme
 * @ingroup comp
 *
 * The default theme is inspired by the material theme.
 *
 * ## Colors
 *
 * The structure of the theming system is heavily based on the base16 system, where 16 colors are mapped as described
 * below:
 *
 * - **base00** - Default Background
 * - **base01** - Lighter Background (Used for status bars, line number and folding marks)
 * - **base02** - Selection Background
 * - **base03** - Comments, Invisibles, Line Highlighting
 * - **base04** - Dark Foreground (Used for status bars)
 * - **base05** - Default Foreground, Caret, Delimiters, Operators
 * - **base06** - Light Foreground (Not often used)
 * - **base07** - Light Background (Not often used)
 * - **base08** - Variables, XML Tags, Markup Link Text, Markup Lists, Diff Deleted
 * - **base09** - Integers, Boolean, Constants, XML Attributes, Markup Link Url
 * - **base0A** - Classes, Markup Bold, Search Text Background
 * - **base0B** - Strings, Inherited Class, Markup Code, Diff Inserted
 * - **base0C** - Support, Regular Expressions, Escape Characters, Markup Quotes
 * - **base0D** - Functions, Methods, Attribute IDs, Headings
 * - **base0E** - Keywords, Storage, Selector, Markup Italic, Diff Changed
 * - **base0F** - Deprecated, Opening/Closing Embedded Language Tags, e.g. `<?php ?>`
 *
 * @see [https://github.com/chriskempson/base16](base16) for more information.
 *
 * @{
 */

#define GUI_THEME_BASE00 GFX_PIXEL(0xFF, 0x21, 0x21, 0x21)
#define GUI_THEME_BASE01 GFX_PIXEL(0xFF, 0x30, 0x30, 0x30)
#define GUI_THEME_BASE02 GFX_PIXEL(0xFF, 0x35, 0x35, 0x35)
#define GUI_THEME_BASE03 GFX_PIXEL(0xFF, 0x4A, 0x4A, 0x4A)
#define GUI_THEME_BASE04 GFX_PIXEL(0xFF, 0xB2, 0xCC, 0xD6)
#define GUI_THEME_BASE05 GFX_PIXEL(0xFF, 0xEE, 0xFF, 0xFF)
#define GUI_THEME_BASE06 GFX_PIXEL(0xFF, 0xEE, 0xFF, 0xFF)
#define GUI_THEME_BASE07 GFX_PIXEL(0xFF, 0xFF, 0xFF, 0xFF)
#define GUI_THEME_BASE08 GFX_PIXEL(0xFF, 0xF0, 0x71, 0x78)
#define GUI_THEME_BASE09 GFX_PIXEL(0xFF, 0xF7, 0x8C, 0x6C)
#define GUI_THEME_BASE0A GFX_PIXEL(0xFF, 0xFF, 0xCB, 0x6B)
#define GUI_THEME_BASE0B GFX_PIXEL(0xFF, 0xC3, 0xE8, 0x8D)
#define GUI_THEME_BASE0C GFX_PIXEL(0xFF, 0x89, 0xDD, 0xFF)
#define GUI_THEME_BASE0D GFX_PIXEL(0xFF, 0x82, 0xAA, 0xFF)
#define GUI_THEME_BASE0E GFX_PIXEL(0xFF, 0xC7, 0x92, 0xEA)
#define GUI_THEME_BASE0F GFX_PIXEL(0xFF, 0xFF, 0x53, 0x70)

#define GUI_THEME_ERROR GUI_THEME_BASE08
#define GUI_THEME_WARNING GUI_THEME_BASE09
#define GUI_THEME_SUCCESS GUI_THEME_BASE0B
#define GUI_THEME_INFO GUI_THEME_BASE0C

#define GUI_THEME_RED GUI_THEME_BASE08
#define GUI_THEME_BRIGHT_RED GUI_THEME_BASE08

#define GUI_THEME_YELLOW GUI_THEME_BASE09
#define GUI_THEME_BRIGHT_YELLOW GUI_THEME_BASE09

#define GUI_THEME_CYAN GUI_THEME_BASE0C
#define GUI_THEME_BRIGHT_CYAN GUI_THEME_BASE0C

#define GUI_THEME_GREEN GUI_THEME_BASE0B
#define GUI_THEME_BRIGHT_GREEN GUI_THEME_BASE0B

#define GUI_THEME_BLUE GUI_THEME_BASE0D
#define GUI_THEME_BRIGHT_BLUE GUI_THEME_BASE0D

#define GUI_THEME_MAGENTA GUI_THEME_BASE0E
#define GUI_THEME_BRIGHT_MAGENTA GUI_THEME_BASE0E

#define GUI_THEME_ALPHA_TRANSPARENT 0x00
#define GUI_THEME_ALPHA_SUBTLE 0x40
#define GUI_THEME_ALPHA_SEMI_TRANSPARENT 0x80
#define GUI_THEME_ALPHA_TRANSLUCENT 0xD0
#define GUI_THEME_ALPHA_OPAQUE 0xFF

#define GUI_THEME_WINDOW_TITLEBAR GUI_THEME_BASE01
#define GUI_THEME_WINDOW_BORDER GUI_THEME_BASE02
#define GUI_THEME_WINDOW_BACKGROUND GUI_THEME_BASE00

#define GUI_THEME_WIDGET_BACKGROUND GUI_THEME_BASE01
#define GUI_THEME_WIDGET_BORDER GUI_THEME_BASE02
#define GUI_THEME_WIDGET_TEXT GUI_THEME_BASE05

#define GUI_THEME_BUTTON_BACKGROUND GUI_THEME_BASE02
#define GUI_THEME_BUTTON_BORDER GUI_THEME_BASE03
#define GUI_THEME_BUTTON_TEXT GUI_THEME_BASE05
#define GUI_THEME_BUTTON_HOVER GUI_THEME_BASE03
#define GUI_THEME_BUTTON_ACTIVE GUI_THEME_BASE01

#define GUI_THEME_WINDOW_TITLEBAR_HEIGHT 24
#define GUI_THEME_WINDOW_BORDER_WIDTH 1
#define GUI_THEME_WINDOW_RADIUS 24

#define GUI_THEME_WIDGET_PADDING 4
#define GUI_THEME_WIDGET_SPACING 4
#define GUI_THEME_WIDGET_RADIUS 2
#define GUI_THEME_WIDGET_BORDER_WIDTH 1

#define GUI_THEME_BUTTON_PADDING_H 8
#define GUI_THEME_BUTTON_PADDING_V 4
#define GUI_THEME_BUTTON_RADIUS 12
#define GUI_THEME_BUTTON_BORDER_WIDTH 1

/** @} */