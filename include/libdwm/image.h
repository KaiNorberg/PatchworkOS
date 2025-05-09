#ifndef DWM_IMAGE_H
#define DWM_IMAGE_H 1

#include "drawable.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct image image_t;

image_t* image_new_blank(display_t* disp, uint64_t width, uint64_t height);

image_t* image_new(display_t* disp, const char* path);

void image_free(image_t* image);

drawable_t* image_draw(image_t* image);

uint64_t image_width(image_t* image);

uint64_t image_height(image_t* image);

#if defined(__cplusplus)
}
#endif

#endif
