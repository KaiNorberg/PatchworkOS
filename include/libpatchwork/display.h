#ifndef PATCHWORK_DISPLAY_H
#define PATCHWORK_DISPLAY_H 1

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

uint64_t display_screen_rect(display_t* disp, rect_t* rect, uint64_t index);

fd_t display_fd(display_t* disp);

bool display_is_connected(display_t* disp);

void display_disconnect(display_t* disp);

bool display_next_event(display_t* disp, event_t* event, clock_t timeout);

void* display_cmds_push(display_t* disp, cmd_type_t type, uint64_t size);

void display_cmds_flush(display_t* disp);

void display_events_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

uint64_t display_wait_for_event(display_t* disp, event_t* event, event_type_t expected);

void display_emit(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

void display_dispatch(display_t* disp, const event_t* event);

void display_subscribe(display_t* disp, event_type_t type);

void display_unsubscribe(display_t* disp, event_type_t type);

void display_get_surface_info(display_t* disp, surface_id_t id, surface_info_t* info);

void display_set_focus(display_t* disp, surface_id_t id);

void display_set_is_visible(display_t* disp, surface_id_t id, bool isVisible);

#if defined(__cplusplus)
}
#endif

#endif
