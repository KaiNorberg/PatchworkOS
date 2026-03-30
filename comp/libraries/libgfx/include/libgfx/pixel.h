#pragma once

#include <stdint.h>

/**
 * @brief Pixel definitions
 * @defgroup comp_libgfx_pixel Pixel
 * @ingroup comp_libgfx
 *
 * @{
 */

/**
 * @brief Pixel structure.
 * @union gfx_pixel_t
 */
typedef union gfx_pixel {
    struct
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    uint32_t argb;
} gfx_pixel_t;

/**
 * @brief Create a pixel from ARGB components.
 *
 * @param _a Alpha component.
 * @param _r Red component.
 * @param _g Green component.
 * @param _b Blue component.
 */
#define GFX_PIXEL(_a, _r, _g, _b) \
    (gfx_pixel_t) \
    { \
        .b = (_b), .g = (_g), .r = (_r), .a = (_a) \
    }

/** @} */