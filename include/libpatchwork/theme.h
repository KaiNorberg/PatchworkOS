#ifndef PATCHWORK_THEME_H
#define PATCHWORK_THEME_H 1

#include "pixel.h"
#include "window.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Theme
 * @defgroup libpatchwork_theme Theme
 * @ingroup libpatchwork
 *
 * @{
 */

/**
 * @brief Invalid color constant.
 */
#define THEME_COLOR_INVALID 0xFFFF00FF

/**
 * @brief Color sets.
 * @enum theme_color_set_t
 *
 * Color sets are used to allow different types of elements to have diferent color schemes.
 */
typedef struct
{
    pixel_t backgroundNormal;
    pixel_t backgroundSelectedStart;
    pixel_t backgroundSelectedEnd;
    pixel_t backgroundUnselectedStart;
    pixel_t backgroundUnselectedEnd;
    pixel_t foregroundNormal;
    pixel_t foregroundInactive;
    pixel_t foregroundLink;
    pixel_t foregroundSelected;
    pixel_t bezel;
    pixel_t highlight;
    pixel_t shadow;
} theme_color_set_t;

/**
 * @brief Number of ANSI colors.
 */
#define THEME_ANSI_COLOR_COUNT 8

/**
 * @brief ANSI colors.
 * @struct theme_ansi_t
 */
typedef struct
{
    pixel_t normal[THEME_ANSI_COLOR_COUNT];
    pixel_t bright[THEME_ANSI_COLOR_COUNT];
    pixel_t dim[THEME_ANSI_COLOR_COUNT];
} theme_ansi_t;

/**
 * @brief Theme structure
 *
 * This structure holds all theme related information.
 */
typedef struct
{
    theme_color_set_t button;
    theme_color_set_t view;
    theme_color_set_t element;
    theme_color_set_t panel;
    theme_color_set_t deco;
    char wallpaper[MAX_NAME];
    char fontsDir[MAX_NAME];
    char cursorArrow[MAX_NAME];
    char defaultFont[MAX_NAME];
    char iconClose[MAX_NAME];
    char iconMinimize[MAX_NAME];
    int64_t frameSize;
    int64_t bezelSize;
    int64_t titlebarSize;
    int64_t panelSize;
    int64_t bigPadding;
    int64_t smallPadding;
    int64_t separatorSize;
    uint8_t reserved[512];
    theme_ansi_t ansi;
} theme_t;

/**
 * @brief Get the global theme.
 *
 * Will potentially load the theme if it is not already loaded.
 *
 * @return The global theme.
 */
theme_t* theme_global_get(void);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
