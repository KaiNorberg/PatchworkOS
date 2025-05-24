#ifndef DWM_FONT_H
#define DWM_FONT_H 1

#include "display.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// NOTE: Font rendering is using our custom format .grf (refer to the readme)

// TODO: Implement font theming, honestly just redo theming entierly
#define FONT_DIR "home:/theme/fonts"
#define FONT_DEFAULT "lato"

typedef struct font font_t;

font_t* font_default(display_t* disp);

font_t* font_new(display_t* disp, const char* family, const char* weight, uint64_t size);

void font_free(font_t* font);

int16_t font_kerning_offset(const font_t* font, char firstChar, char secondChar);

uint64_t font_width(const font_t* font, const char* string, uint64_t length);

uint64_t font_height(const font_t* font);

#if defined(__cplusplus)
}
#endif

#endif
