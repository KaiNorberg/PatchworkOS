#ifndef DWM_ELEMENT_H
#define DWM_ELEMENT_H 1

#include "cmd.h"
#include "drawable.h"
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

typedef struct element element_t;

element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, procedure_t procedure, void* private);

void element_free(element_t* elem);

element_t* element_find(element_t* elem, element_id_t id);

void element_set_private(element_t* elem, void* private);

void* element_private(element_t* elem);

element_id_t element_id(element_t* elem);

void element_rect(element_t* elem, rect_t* rect);

void element_content_rect(element_t* elem, rect_t* rect);

void element_global_rect(element_t* elem, rect_t* rect);

void element_global_point(element_t* elem, point_t* point);

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src);

void element_point_to_global(element_t* elem, point_t* dest, const point_t* src);

void element_global_to_rect(element_t* elem, rect_t* dest, const rect_t* src);

void element_global_to_point(element_t* elem, point_t* dest, const point_t* src);

void element_draw_begin(element_t* elem, drawable_t* draw);

void element_draw_end(element_t* elem, drawable_t* draw);

void element_send_redraw(element_t* elem, bool propagate);

uint64_t element_dispatch(element_t* elem, const event_t* event);

#if defined(__cplusplus)
}
#endif

#endif
