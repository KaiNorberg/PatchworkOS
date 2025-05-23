#ifndef DWM_FONT_H
#define DWM_FONT_H 1

#include "display.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// NOTE: Currently font rendering is limited to psf fonts.

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864AB572
#define PSF1_MODE_512 (1 << 0)

typedef struct font font_t;

font_t* font_default(display_t* disp);

font_t* font_new(display_t* disp, const char* path, uint64_t desiredHeight);

void font_free(font_t* font);

uint64_t font_width(font_t* font);

uint64_t font_height(font_t* font);

#if defined(__cplusplus)
}
#endif

#endif
