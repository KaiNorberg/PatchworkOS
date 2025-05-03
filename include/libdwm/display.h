#ifndef DWM_DISPLAY_H
#define DWM_DISPLAY_H 1

#include "cmd.h"
#include "event.h"
#include "rect.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct display display_t;

display_t* display_new(void);

void display_free(display_t* disp);

surface_id_t display_gen_id(display_t* disp);

uint64_t display_screen_rect(display_t* disp, rect_t* rect, uint64_t index);

bool display_connected(display_t* disp);

bool display_next_event(display_t* disp, event_t* event, nsec_t timeout);

void display_cmds_push(display_t* disp, const cmd_header_t* cmd);

void display_cmds_flush(display_t* disp);

void display_events_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

uint64_t display_send_recieve(display_t* disp, cmd_header_t* cmd, event_t* event, event_type_t expected);

void display_emit(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

void display_dispatch(display_t* disp, const event_t* event);

#if defined(__cplusplus)
}
#endif

#endif
