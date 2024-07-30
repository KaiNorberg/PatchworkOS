#ifndef _SYS_DWM_H
#define _SYS_DWM_H 1

#include <stdint.h>
#include <sys/io.h>
#include <sys/mouse.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/nsec_t.h"
#include "_AUX/point_t.h"

typedef enum
{
    DWM_WINDOW = 0,
    DWM_FULLSCREEN = 1,
    DWM_PANEL = 2,
    DWM_CURSOR = 3,
    DWM_WALL = 4,
    DWM_MAX = 4
} dwm_type_t;

#define DWM_MAX_NAME 32

#define MSG_MAX_DATA 64

typedef uint16_t msg_type_t;

typedef struct
{
    nsec_t time;
    msg_type_t type;
    uint8_t data[MSG_MAX_DATA]; // Stores the *msg_*_t structs.
} msg_t;

typedef struct
{
    mouse_buttons_t held;
    mouse_buttons_t pressed;
    mouse_buttons_t released;
    point_t pos;
    point_t delta;
} msg_mouse_t;

#define MSG_NONE 0
#define MSG_KBD 1
#define MSG_MOUSE 2
#define MSG_SELECT 3
#define MSG_DESELECT 4

#define MSG_INIT(msgType, msgData) \
    ({ \
        msg_t msg = (msg_t){.type = (msgType)}; \
        memcpy(msg.data, (msgData), sizeof(*(msgData))); \
        msg; \
    })

typedef struct ioctl_dwm_create
{
    point_t pos;
    uint32_t width;
    uint32_t height;
    dwm_type_t type;
    char name[DWM_MAX_NAME];
} ioctl_dwm_create_t;

typedef struct ioctl_dwm_size
{
    uint32_t outWidth;
    uint32_t outHeight;
} ioctl_dwm_size_t;

#define IOCTL_DWM_CREATE 0
#define IOCTL_DWM_SIZE 1

typedef struct ioctl_window_receive
{
    nsec_t timeout;
    msg_t outMsg;
} ioctl_window_receive_t;

typedef struct ioctl_window_send
{
    msg_t msg;
} ioctl_window_send_t;

typedef struct ioctl_window_move
{
    point_t pos;
    uint32_t width;
    uint32_t height;
} ioctl_window_move_t;

#define IOCTL_WINDOW_RECEIVE 0
#define IOCTL_WINDOW_SEND 1
#define IOCTL_WINDOW_MOVE 2

#endif
