#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/nsec_t.h"
#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

typedef uint64_t win_flag_t;

#define WIN_SIZE(info) ((info)->width * (info)->height * sizeof(pixel_t))

#ifndef _WIN_INTERNAL
typedef void win_t;
#endif

typedef uint64_t msg_t;

typedef struct msg_kbd
{
    nsec_t time;
    uint8_t type;
    uint8_t code;
} msg_kbd_t;

#define MSG_NONE 0
#define MSG_INIT 1
#define MSG_QUIT 2
#define MSG_KBD 3
#define MSG_MOUSE 4
#define MSG_USER (1ULL << 63)

typedef struct ioctl_win_init
{
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
} ioctl_win_init_t;

typedef struct ioctl_win_dispatch
{
    nsec_t timeout;
    msg_t type;
    uint8_t data[48];
} ioctl_win_dispatch_t;

#define IOCTL_WIN_INIT 0
#define IOCTL_WIN_DISPATCH 1

typedef uint64_t (*procedure_t)(win_t*, msg_t, void* data);

win_t* win_new(ioctl_win_init_t* info, procedure_t procedure, win_flag_t flags);

uint64_t win_free(win_t* window);

uint64_t win_flush(win_t* window);

void win_screen_rect(win_t* window, rect_t* rect);

void win_local_rect(win_t* window, rect_t* rect);

msg_t win_dispatch(win_t* window, nsec_t timeout);

uint64_t win_send(win_t* window, msg_t msg, void* data);

uint64_t gfx_bitfield(win_t* window, const rect_t* rect, const void* buffer);

uint64_t gfx_rect(win_t* window, const rect_t* rect, pixel_t pixel);

#if defined(__cplusplus)
}
#endif

#endif
