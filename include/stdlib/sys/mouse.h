#ifndef _SYS_KEYBOARD_H
#define _SYS_KEYBOARD_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/nsec_t.h"

typedef struct mouse_event
{
    nsec_t time;
    uint8_t buttons;
    int16_t deltaX;
    int16_t deltaY;
} mouse_event_t;

#define MOUSE_RIGHT (1 << 0)
#define MOUSE_MIDDLE (1 << 1)
#define MOUSE_LEFT (1 << 2)

#if defined(__cplusplus)
}
#endif

#endif
