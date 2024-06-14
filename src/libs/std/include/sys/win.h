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

typedef uint64_t msg_t;

typedef struct msg_kbd
{
    nsec_t time;
    uint8_t type;
    uint8_t code;
} msg_kbd_t;

#define MSG_MAX_DATA 48

// Kernel messages
#define MSG_NONE 0
#define MSG_KBD 1
#define MSG_MOUSE 2

// Library messages
#define LMSG_BASE (1ULL << 62)
#define LMSG_INIT (LMSG_BASE + 0)
#define LMSG_QUIT (LMSG_BASE + 1)
#define LMSG_REDRAW (LMSG_BASE + 2)

// User messages
#define UMSG_BASE (1ULL << 63)

typedef struct ioctl_win_init
{
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
} ioctl_win_init_t;

typedef struct ioctl_win_receive
{
    nsec_t timeout;
    msg_t type;
    uint8_t data[MSG_MAX_DATA];
} ioctl_win_receive_t;

typedef struct ioctl_win_send
{
    msg_t type;
    uint8_t data[MSG_MAX_DATA];
} ioctl_win_send_t;

typedef struct ioctl_win_move
{
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
} ioctl_win_move_t;

#define IOCTL_WIN_INIT 0
#define IOCTL_WIN_RECEIVE 1
#define IOCTL_WIN_SEND 2
#define IOCTL_WIN_MOVE 3

#ifndef _WIN_INTERNAL
typedef void win_t;
#endif

typedef uint64_t win_flag_t;

#define WIN_NONE 0
#define WIN_DECO (1 << 0)

typedef uint64_t (*procedure_t)(win_t*, msg_t, void* data);

void win_client_to_window(rect_t* rect, win_flag_t flags);

void win_window_to_client(rect_t* rect, win_flag_t flags);

win_t* win_new(const rect_t* rect, procedure_t procedure, win_flag_t flags);

uint64_t win_free(win_t* window);

uint64_t win_flush(win_t* window);

msg_t win_dispatch(win_t* window, nsec_t timeout);

uint64_t win_send(win_t* window, msg_t type, void* data, uint64_t size);

uint64_t win_move(win_t* window, const rect_t* rect);

void win_window_area(win_t* window, rect_t* rect);

void win_client_area(win_t* window, rect_t* rect);

void win_draw_rect(win_t* window, const rect_t* rect, pixel_t pixel);

#if defined(__cplusplus)
}
#endif

#endif
