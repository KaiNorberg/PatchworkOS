#ifndef DWM_PROCEDURE_H
#define DWM_PROCEDURE_H 1

#include "event.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct window window_t;
typedef struct element element_t;
typedef uint64_t (*procedure_t)(window_t*, element_t*, const event_t*);

#if defined(__cplusplus)
}
#endif

#endif
