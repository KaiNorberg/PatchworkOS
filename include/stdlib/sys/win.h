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

typedef struct msg_mouse
{
    nsec_t time;
    uint8_t buttons;
    point_t pos;
    int16_t deltaX;
    int16_t deltaY;
} msg_mouse_t;

#define MSG_MAX_DATA 48

// Kernel messages
#define MSG_NONE 0
#define MSG_KEYBOARD 1
#define MSG_MOUSE 2
#define MSG_SELECT 3
#define MSG_DESELECT 4

// Library messages
#define LMSG_BASE (1ULL << 62)
#define LMSG_INIT (LMSG_BASE + 0)
#define LMSG_QUIT (LMSG_BASE + 1)
#define LMSG_REDRAW (LMSG_BASE + 2)

// User messages
#define UMSG_BASE (1ULL << 63)

typedef uint8_t win_type_t;

#define WIN_WINDOW 0
#define WIN_FULLSCREEN 1 // NOT IMPLEMENTED
#define WIN_PANEL 2
#define WIN_CURSOR 3
#define WIN_WALL 4
#define WIN_MAX WIN_WALL

typedef struct ioctl_dwm_create
{
    point_t pos;
    uint32_t width;
    uint32_t height;
    win_type_t type;
    char name[MAX_PATH];
} ioctl_dwm_create_t;

typedef struct ioctl_dwm_size
{
    uint32_t outWidth;
    uint32_t outHeight;
} ioctl_dwm_size_t;

#define IOCTL_DWM_CREATE 0
#define IOCTL_DWM_SIZE 1

typedef struct ioctl_win_receive
{
    nsec_t timeout;
    msg_t outType;
    uint8_t outData[MSG_MAX_DATA];
} ioctl_win_receive_t;

typedef struct ioctl_win_send
{
    msg_t type;
    uint8_t data[MSG_MAX_DATA];
} ioctl_win_send_t;

typedef struct ioctl_win_move
{
    int64_t x;
    int64_t y;
    uint32_t width;
    uint32_t height;
} ioctl_win_move_t;

#define IOCTL_WIN_RECEIVE 0
#define IOCTL_WIN_SEND 1
#define IOCTL_WIN_MOVE 2

#ifndef _WIN_INTERNAL
typedef uint8_t win_t;
#endif

typedef struct win_theme
{
    uint32_t edgeWidth;
    uint32_t ridgeWidth;
    pixel_t highlight;
    pixel_t shadow;
    pixel_t background;
    pixel_t selected;
    pixel_t unSelected;
    uint64_t topbarHeight;
} win_theme_t;

typedef uint64_t (*procedure_t)(win_t*, surface_t*, msg_t, void* data);

void win_default_theme(win_theme_t* theme);

uint64_t win_screen_rect(rect_t* rect);

void win_client_to_window(rect_t* rect, const win_theme_t* theme, win_type_t type);

void win_window_to_client(rect_t* rect, const win_theme_t* theme, win_type_t type);

win_t* win_new(const char* name, const rect_t* rect, const win_theme_t* theme, procedure_t procedure, win_type_t type);

uint64_t win_free(win_t* window);

msg_t win_dispatch(win_t* window, nsec_t timeout);

uint64_t win_send(win_t* window, msg_t type, void* data, uint64_t size);

uint64_t win_move(win_t* window, const rect_t* rect);

void win_window_area(win_t* window, rect_t* rect);

void win_client_area(win_t* window, rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
