#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include "dwm.h"
#include "pixel.h"
#include "point.h"
#include "rect.h"
#include "display.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef _WIN_INTERNAL
typedef uint8_t win_t;
#endif

typedef enum win_flags
{
    WIN_NONE = 0,
    WIN_DECO = (1 << 0)
} win_flags_t;

typedef uint64_t (*win_proc_t)(win_t*, const event_t*);

win_t* win_new(display_t* disp, const char* name, const rect_t* rect, dwm_type_t type, win_flags_t flags, win_proc_t* procedure);

#if defined(__cplusplus)
}
#endif

#endif
