#ifndef _SYS_PIXEL_H
#define _SYS_PIXEL_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint32_t pixel_t;

#define PIXEL_ALPHA(pixel) (((pixel) >> 24) & 0xFF)
#define PIXEL_RED(pixel) (((pixel) >> 16) & 0xFF)
#define PIXEL_GREEN(pixel) (((pixel) >> 8) & 0xFF)
#define PIXEL_BLUE(pixel) (((pixel) >> 0) & 0xFF)

#define PIXEL_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))

#define PIXEL_BLEND(dest, src) \
    ({ \
        uint8_t aAlpha = PIXEL_ALPHA(*src); \
        uint8_t bAlpha = PIXEL_ALPHA(*dest); \
        uint8_t alpha = aAlpha + ((bAlpha * (0xFF - aAlpha)) / 0xFF); \
        if (alpha != 0) \
        { \
            *dest = PIXEL_ARGB(alpha, (PIXEL_RED(*src) * aAlpha + PIXEL_RED(*dest) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha, \
                (PIXEL_GREEN(*src) * aAlpha + PIXEL_GREEN(*dest) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha, \
                (PIXEL_BLUE(*src) * aAlpha + PIXEL_BLUE(*dest) * bAlpha * (0xFF - aAlpha) / 0xFF) / alpha); \
        } \
        else \
        { \
            *dest = 0; \
        } \
    })

#if defined(__cplusplus)
}
#endif

#endif
