#ifndef __EMBED__

#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/mouse.h>
#include <sys/win.h>

typedef struct
{
    win_text_prop_t props;
} label_t;

uint64_t win_label_proc(widget_t* widget, win_t* window, const msg_t* msg)
{
    static win_theme_t theme;

    switch (msg->type)
    {
    case WMSG_INIT:
    {
        win_theme(&theme);

        label_t* label = malloc(sizeof(label_t));
        label->props = WIN_TEXT_PROP_DEFAULT();
        win_widget_private_set(widget, label);
    }
    break;
    case WMSG_FREE:
    {
        free(win_widget_private(widget));
    }
    break;
    case WMSG_LABEL_PROP:
    {
        wmsg_label_prop_t* data = (wmsg_label_prop_t*)msg->data;
        label_t* label = win_widget_private(widget);
        label->props = data->props;
    }
    break;
    case WMSG_REDRAW:
    {
        label_t* label = win_widget_private(widget);

        rect_t rect;
        win_widget_rect(widget, &rect);

        gfx_t gfx;
        win_draw_begin(window, &gfx);

        gfx_edge(&gfx, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
        RECT_SHRINK(&rect, theme.edgeWidth);
        gfx_rect(&gfx, &rect, theme.bright);
        RECT_SHRINK(&rect, theme.edgeWidth);
        rect.top += theme.edgeWidth;
        gfx_psf(&gfx, win_font(window), &rect, label->props.xAlign, label->props.yAlign, label->props.height,
            win_widget_name(widget), label->props.foreground, label->props.background);

        win_draw_end(window, &gfx);
    }
    break;
    }

    return 0;
}

widget_t* win_label_new(win_t* window, const char* name, const rect_t* rect, widget_id_t id, win_text_prop_t* textProp)
{
    widget_t* label = win_widget_new(window, win_label_proc, name, rect, id);

    wmsg_label_prop_t props = {.props = *textProp};
    win_widget_send(label, WMSG_LABEL_PROP, &props, sizeof(wmsg_label_prop_t));

    return label;
}

#endif
