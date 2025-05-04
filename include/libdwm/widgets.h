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

typedef struct
{
    font_t* font;
    pixel_t foreground;
    pixel_t background;
    button_flags_t flags;
} button_props_t;

button_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, pixel_t foreground,
    pixel_t background, button_flags_t flags, const char* text);

void button_free(button_t* button);

font_t* button_font(button_t* button);
void button_set_font(button_t* button, font_t* font);

pixel_t button_foreground(button_t* button);
void button_set_foreground(button_t* button, pixel_t foreground);

pixel_t button_background(button_t* button);
void button_set_background(button_t* button, pixel_t background);

button_flags_t button_flags(button_t* button);
void button_set_flags(button_t* button, button_flags_t flags);

const char* button_text(button_t* button);
uint64_t button_set_text(button_t* button, const char* text);

typedef struct label label_t;

typedef enum
{
    LABEL_NONE,
    LABEL_FLAT,
} label_flags_t;

label_t* label_new(element_t* parent, element_id_t id, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, label_flags_t flags, const char* text);

font_t* label_font(label_t* label);
void label_set_font(label_t* label, font_t* font);

align_t label_xalign(label_t* label);
void label_set_xalign(label_t* label, align_t xAlign);

align_t label_yalign(label_t* label);
void label_set_yalign(label_t* label, align_t yAlign);

pixel_t label_foreground(label_t* label);
void label_set_foreground(label_t* label, pixel_t foreground);

pixel_t label_background(label_t* label);
void label_set_background(label_t* label, pixel_t background);

label_flags_t label_flags(label_t* label);
void label_set_flags(label_t* label, label_flags_t flags);

const char* label_text(label_t* label);
uint64_t label_set_text(label_t* label, const char* text);

#if defined(__cplusplus)
}
#endif

#endif
