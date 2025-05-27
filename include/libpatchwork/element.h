#ifndef PATCHWORK_ELEMENT_H
#define PATCHWORK_ELEMENT_H 1

#include "cmd.h"
#include "drawable.h"
#include "element_id.h"
#include "font.h"
#include "procedure.h"
#include "rect.h"
#include "theme.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// We make this a uint64_t instead an an enum to give us more flags.
typedef uint64_t element_flags_t;

#define ELEMENT_NONE 0
#define ELEMENT_TOGGLE (1 << 0)
#define ELEMENT_FLAT (1 << 1)
#define ELEMENT_NO_BEZEL (1 << 2)
#define ELEMENT_NO_OUTLINE (1 << 3)

typedef struct element element_t;

#define ELEMENT_TEXT_PROPS_DEFAULT \
    (element_text_props_t) \
    { \
        .font = NULL, .xAlign = ALIGN_CENTER, .yAlign = ALIGN_CENTER \
    }

#define ELEMENT_IMAGE_PROPS_DEFAULT \
    (element_image_props_t) \
    { \
        .xAlign = ALIGN_CENTER, .yAlign = ALIGN_CENTER, .srcOffset = (point_t){0} \
    }

typedef struct
{
    font_t* font;
    align_t xAlign;
    align_t yAlign;
} element_text_props_t;

typedef struct
{
    align_t xAlign;
    align_t yAlign;
    point_t srcOffset;
} element_image_props_t;

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private);

void element_free(element_t* elem);

element_t* element_find(element_t* elem, element_id_t id);

void element_private_set(element_t* elem, void* private);
void* element_private_get(element_t* elem);

element_id_t element_id_get(element_t* elem);

void element_rect_get(element_t* elem, rect_t* rect);
void element_rect_set(element_t* elem, const rect_t* rect);

void element_content_rect_get(element_t* elem, rect_t* rect);

void element_global_rect_get(element_t* elem, rect_t* rect);

void element_global_point_get(element_t* elem, point_t* point);

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src);

void element_point_to_global(element_t* elem, point_t* dest, const point_t* src);

void element_global_to_rect(element_t* elem, rect_t* dest, const rect_t* src);

void element_global_to_point(element_t* elem, point_t* dest, const point_t* src);

void element_draw_begin(element_t* elem, drawable_t* draw);

void element_draw_end(element_t* elem, drawable_t* draw);

void element_send_redraw(element_t* elem, bool propagate);

uint64_t element_dispatch(element_t* elem, const event_t* event);

uint64_t element_emit(element_t* elem, event_type_t type, void* data, uint64_t size);

element_flags_t element_flags_get(element_t* elem);
void element_flags_set(element_t* elem, element_flags_t flags);

const char* element_text_get(element_t* elem);
uint64_t element_text_set(element_t* elem, const char* text);

element_text_props_t* element_text_props_get(element_t* elem);

image_t* element_image_get(element_t* elem);
void element_image_set(element_t* elem, image_t* image);

element_image_props_t* element_image_props_get(element_t* elem);

pixel_t element_color_get(element_t* elem, theme_color_set_t set, theme_color_role_t role);
uint64_t element_color_set(element_t* elem, theme_color_set_t set, theme_color_role_t role, pixel_t color);

const char* element_string_get(element_t* elem, theme_string_t name);
uint64_t element_string_set(element_t* elem, theme_string_t name, const char* string);

int64_t element_int_get(element_t* elem, theme_int_t name);
uint64_t element_int_set(element_t* elem, theme_int_t name, int64_t integer);

#if defined(__cplusplus)
}
#endif

#endif
