#ifndef _AUX_PIXEL_T_H
#define _AUX_PIXEL_T_H 1

#include <stdint.h>

typedef uint32_t pixel_t;

#define PIXEL_ALPHA(pixel) (((pixel) >> 24) & 0xFF)
#define PIXEL_RED(pixel) (((pixel) >> 16) & 0xFF)
#define PIXEL_GREEN(pixel) (((pixel) >> 8) & 0xFF)
#define PIXEL_BLUE(pixel) (((pixel) >> 0) & 0xFF)

#define PIXEL_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))

#endif
