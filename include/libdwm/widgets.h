#ifndef DWM_WIDGETS_H
#define DWM_WIDGETS_H 1

#include <stdint.h>

#include "element.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct button button_t;

typedef enum
{
    BUTTON_NONE,
    BUTTON_TOGGLE,
} button_flags_t;

button_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, pixel_t foreground, pixel_t background,
    button_flags_t flags, const char* label);

void button_free(button_t* button);

const char* button_label(button_t* button);

uint64_t button_set_label(button_t* button, const char* label);

font_t* button_font(button_t* button);

void button_set_font(button_t* button, font_t* font);

#if defined(__cplusplus)
}
#endif

#endif
