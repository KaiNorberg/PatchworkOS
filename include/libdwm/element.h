#ifndef DWM_ELEMENT_H
#define DWM_ELEMENT_H 1

#include "cmd.h"
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

element_t* element_new(element_t* parent, const rect_t* rect, procedure_t procedure);

void element_free(element_t* elem);

void element_rect(element_t* elem, rect_t* rect);

void element_content_rect(element_t* elem, rect_t* rect);

void element_global_rect(element_t* elem, rect_t* rect);

void element_rect_to_global(element_t* elem, rect_t* dest, const rect_t* src);

void element_draw_rect(element_t* elem, const rect_t* rect, pixel_t pixel);

void element_draw_edge(element_t* elem, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void element_draw_gradient(element_t* elem, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise);

#if defined(__cplusplus)
}
#endif

#endif
