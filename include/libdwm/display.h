#ifndef _DWM_DISPLAY_H
#define _DWM_DISPLAY_H 1

#include "rect.h"
#include "cmd.h"
#include "event.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct display display_t;

display_t* display_open(void);

void display_close(display_t* disp);

void display_screen_rect(display_t* disp, rect_t* rect, uint64_t index);

bool display_connected(display_t* disp);

bool display_next_event(display_t* disp, event_t* event, nsec_t timeout);

void display_dispatch(display_t* disp, event_t* event);

#if defined(__cplusplus)
}
#endif

#endif
