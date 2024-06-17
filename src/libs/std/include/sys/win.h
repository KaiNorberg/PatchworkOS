#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/gfx.h>
#include <sys/io.h>

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

typedef struct ioctl_dwm_create
{
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
    char name[MAX_PATH];
} ioctl_dwm_create_t;

#define IOCTL_DWM_CREATE 0

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

#define IOCTL_WIN_RECEIVE 0
#define IOCTL_WIN_SEND 1
#define IOCTL_WIN_MOVE 2

#ifndef _WIN_INTERNAL
typedef uint8_t win_t;
#endif

typedef uint64_t win_flag_t;

#define WIN_NONE 0
#define WIN_DECO (1 << 0)
#define WIN_NORESIZE (1 << 1)

typedef struct win_theme
{
    uint64_t edgeWidth;
    pixel_t highlight;
    pixel_t shadow;
    pixel_t background;
    pixel_t wall;
    pixel_t topbarHighlight;
    pixel_t topbarShadow;
    uint64_t topbarHeight;
} win_theme_t;

typedef uint64_t (*procedure_t)(win_t*, msg_t, void* data);

void win_default_theme(win_theme_t* theme);

void win_client_to_window(rect_t* rect, const win_theme_t* theme, win_flag_t flags);

void win_window_to_client(rect_t* rect, const win_theme_t* theme, win_flag_t flags);

win_t* win_new(const char* name, const rect_t* rect, const win_theme_t* theme, procedure_t procedure, win_flag_t flags);

uint64_t win_free(win_t* window);

msg_t win_dispatch(win_t* window, nsec_t timeout);

uint64_t win_send(win_t* window, msg_t type, void* data, uint64_t size);

uint64_t win_move(win_t* window, const rect_t* rect);

void win_window_area(win_t* window, rect_t* rect);

void win_client_area(win_t* window, rect_t* rect);

void win_window_surface(win_t* window, surface_t* surface);

void win_client_surface(win_t* window, surface_t* surface);

#if defined(__cplusplus)
}
#endif

#endif
