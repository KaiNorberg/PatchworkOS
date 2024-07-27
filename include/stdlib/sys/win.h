#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/dwm.h>
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

#ifndef _WIN_INTERNAL
typedef uint8_t win_t;
typedef uint8_t widget_t;
#endif

typedef enum win_flags
{
    WIN_NONE = 0,
    WIN_DECO = (1 << 0)
} win_flags_t;

typedef uint16_t widget_id_t;

typedef struct win_theme
{
    uint8_t edgeWidth;
    uint8_t rimWidth;
    uint8_t ridgeWidth;
    pixel_t highlight;
    pixel_t shadow;
    pixel_t bright;
    pixel_t dark;
    pixel_t background;
    pixel_t selected;
    pixel_t unSelected;
    uint8_t topbarHeight;
    uint8_t topbarPadding;
} win_theme_t;

typedef struct win_text_prop
{
    uint64_t height;
    pixel_t foreground;
    pixel_t background;
    gfx_align_t xAlign;
    gfx_align_t yAlign;
} win_text_prop_t;

#define WIN_TEXT_PROP_DEFAULT() \
    (win_text_prop_t) \
    { \
        .height = 16, .foreground = 0xFF000000, .background = 0, .xAlign = GFX_CENTER, .yAlign = GFX_CENTER, \
    }

// Library messages
typedef struct
{
    const char* name;
    dwm_type_t type;
    uint8_t rectIsClient;
    rect_t rect;
    void* private;
} lmsg_init_t;

typedef enum lmsg_button_type
{
    LMSG_BUTTON_RELEASED = 0,
    LMSG_BUTTON_PRESS = 1
} lmsg_button_type_t;

typedef struct
{
    lmsg_button_type_t type;
    widget_id_t id;
} lmsg_button_t;

#define LMSG_BASE (1 << 14)
#define LMSG_INIT (LMSG_BASE + 1)
#define LMSG_QUIT (LMSG_BASE + 2)
#define LMSG_REDRAW (LMSG_BASE + 3)
#define LMSG_BUTTON (LMSG_BASE + 4)

// Widget messages
typedef msg_mouse_t wmsg_mouse_t;
typedef win_text_prop_t wmsg_text_prop_t;

#define WMSG_BASE (1 << 15)
#define WMSG_INIT (WMSG_BASE + 0)
#define WMSG_FREE (WMSG_BASE + 1)
#define WMSG_REDRAW (WMSG_BASE + 2)
#define WMSG_MOUSE (WMSG_BASE + 3)
#define WMSG_TEXT_PROP (WMSG_BASE + 4)

// User messages
#define UMSG_BASE ((1 << 15) | (1 << 14))

typedef uint64_t (*win_proc_t)(win_t*, const msg_t*);
typedef uint64_t (*widget_proc_t)(widget_t*, win_t*, const msg_t*);

win_t* win_new(const char* name, const rect_t* rect, dwm_type_t type, win_flags_t flags, win_proc_t procedure);

uint64_t win_free(win_t* window);

uint64_t win_poll(win_t** windows, uint64_t amount, nsec_t timeout);

uint64_t win_send(win_t* window, msg_type_t type, const void* data, uint64_t size);

uint64_t win_receive(win_t* window, msg_t* msg, nsec_t timeout);

uint64_t win_dispatch(win_t* window, const msg_t* msg);

uint64_t win_draw_begin(win_t* window, gfx_t* gfx);

uint64_t win_draw_end(win_t* window, gfx_t* gfx);

uint64_t win_move(win_t* window, const rect_t* rect);

const char* win_name(win_t* window);

void win_screen_window_rect(win_t* window, rect_t* rect);

void win_screen_client_rect(win_t* window, rect_t* rect);

void win_client_rect(win_t* window, rect_t* rect);

void win_screen_to_window(win_t* window, point_t* point);

void win_screen_to_client(win_t* window, point_t* point);

void win_window_to_client(win_t* window, point_t* point);

gfx_psf_t* win_font(win_t* window);

uint64_t win_font_set(win_t* window, const char* path);

widget_t* win_widget(win_t* window, widget_id_t id);

widget_t* win_widget_new(win_t* window, widget_proc_t procedure, const char* name, const rect_t* rect, widget_id_t id);

void win_widget_free(widget_t* widget);

uint64_t win_widget_send(widget_t* widget, msg_type_t type, const void* data, uint64_t size);

uint64_t win_widget_send_all(win_t* window, msg_type_t type, const void* data, uint64_t size);

void win_widget_rect(widget_t* widget, rect_t* rect);

widget_id_t win_widget_id(widget_t* widget);

const char* win_widget_name(widget_t* widget);

void win_widget_name_set(widget_t* widget, const char* name);

void* win_widget_private(widget_t* widget);

void win_widget_private_set(widget_t* widget, void* private);

uint64_t win_screen_rect(rect_t* rect);

void win_theme(win_theme_t* out);

void win_expand_to_window(rect_t* clientRect, win_flags_t flags);

void win_shrink_to_client(rect_t* windowRect, win_flags_t flags);

uint64_t win_button_proc(widget_t* widget, win_t* window, const msg_t* msg);

uint64_t win_label_proc(widget_t* widget, win_t* window, const msg_t* msg);

#if defined(__cplusplus)
}
#endif

#endif
