#ifndef _SYS_DISPLAY_H
#define _SYS_DISPLAY_H 1

#include <stdint.h>

#include "rect.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef _WIN_INTERNAL
typedef uint8_t display_t;
#endif

display_t* display_open(void);

void display_close(display_t* disp);

void display_screen_rect(display_t* disp, rect_t* rect, uint64_t index);

#if defined(__cplusplus)
}
#endif

#endif
