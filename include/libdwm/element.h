#ifndef DWM_ELEMENT_H
#define DWM_ELEMENT_H 1

#include "cmd.h"
#include "element_id.h"
#include "font.h"
#include "procedure.h"
#include "rect.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    ALIGN_CENTER = 0,
    ALIGN_MAX = 1,
    ALIGN_MIN = 2,
} align_t;

typedef struct element element_t;

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, procedure_t procedure, void* private);

void element_free(element_t* elem);

element_t* element_find(element_t* elem, element_id_t id);

void element_set_private(element_t* elem, void* private);

void* element_private(element_t* elem);

void element_rect(element_t* elem, rect_t* rect);

void element_content_rect(element_t* elem, rect_t* rect);

void element_global_rect(element_t* elem, rect_t* rect);

void element_global_point(element_t* elem, point_t* point);

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src);

void element_point_to_global(element_t* elem, point_t* dest, const point_t* src);

void element_global_to_rect(element_t* elem, rect_t* dest, const rect_t* src);

void element_global_to_point(element_t* elem, point_t* dest, const point_t* src);

void element_invalidate(element_t* elem, const rect_t* rect);

void element_draw_rect(element_t* elem, const rect_t* rect, pixel_t pixel);

void element_draw_edge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void element_draw_gradient(element_t* elem, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise);

void element_draw_transfer(element_t* elem, const rect_t* destRect, const point_t* srcPoint);

void element_draw_rim(element_t* elem, const rect_t* rect, uint64_t width, pixel_t pixel);

void element_draw_string(element_t* elem, font_t* font, const point_t* point, pixel_t foreground, pixel_t background,
    const char* string, uint64_t length);

void element_draw_text(element_t* elem, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, const char* text);

void element_draw_ridge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void element_send_redraw(element_t* elem, bool propagate);

uint64_t element_dispatch(element_t* elem, const event_t* event);

#if defined(__cplusplus)
}
#endif

#endif
