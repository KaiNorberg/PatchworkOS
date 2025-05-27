#ifndef PATCHWORK_WIDGETS_H
#define PATCHWORK_WIDGETS_H 1

#include <stdint.h>

#include "drawable.h"
#include "element.h"

#if defined(__cplusplus)
extern "C"
{
#endif

element_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags);

element_t* label_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags);

#if defined(__cplusplus)
}
#endif

#endif
