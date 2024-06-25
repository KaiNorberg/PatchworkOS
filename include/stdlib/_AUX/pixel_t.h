#ifndef _AUX_PIXEL_T_H
#define _AUX_PIXEL_T_H 1

#include <stdint.h>

typedef uint32_t pixel_t;

#define PIXEL_ALPHA(pixel) (((pixel) >> 24) & 0xFF)
#define PIXEL_RED(pixel) (((pixel) >> 16) & 0xFF)
#define PIXEL_GREEN(pixel) (((pixel) >> 8) & 0xFF)
#define PIXEL_BLUE(pixel) (((pixel) >> 0) & 0xFF)

#define PIXEL_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))

#define PIXEL_BLEND(a, b) \
    ({ \
        uint8_t aAlpha = PIXEL_ALPHA(a); \
        uint8_t bAlpha = PIXEL_ALPHA(b); \
        uint8_t alpha = aAlpha + ((bAlpha * (0xFF - aAlpha)) / 0xFF); \
        pixel_t result; \
        if (alpha != 0) \
        { \
            result = PIXEL_ARGB(alpha, (PIXEL_RED(a) * aAlpha + PIXEL_RED(b) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha, \
                (PIXEL_GREEN(a) * aAlpha + PIXEL_GREEN(b) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha, \
                (PIXEL_BLUE(a) * aAlpha + PIXEL_BLUE(b) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha); \
        } \
        else \
        { \
            result = 0; \
        } \
        result; \
    })

#endif
