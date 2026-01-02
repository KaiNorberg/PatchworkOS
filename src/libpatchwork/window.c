#include <libpatchwork/display.h>
#include <libpatchwork/image.h>
#define __STDC_WANT_LIB_EXT1__ 1
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/defs.h>

#define WINDOW_CLIENT_ELEM_ID (UINT64_MAX)
#define WINDOW_DECO_ELEM_ID (UINT64_MAX - 1)
#define WINDOW_DECO_CLOSE_BUTTON_ID (UINT64_MAX - 2)
#define WINDOW_DECO_MINIMIZE_BUTTON_ID (UINT64_MAX - 3)

#define WINDOW_DECO_CLOSE_BUTTON_INDEX 0
#define WINDOW_DECO_MINIMIZE_BUTTON_INDEX 1
#define WINDOW_DECO_BUTTON_AMOUNT 2

typedef struct
{
    bool isFocused;
    bool isVisible;
    bool isDragging;
    point_t dragOffset;
    image_t* closeIcon;
    image_t* minimizeIcon;
} deco_private_t;

static void window_deco_titlebar_rect(window_t* win, element_t* elem, rect_t* rect)
{
    UNUSED(win);

    rect_t contentRect = element_get_content_rect(elem);
    const theme_t* theme = element_get_theme(elem);

    *rect = (rect_t){
        .left = theme->frameSize + theme->smallPadding,
        .top = theme->frameSize + theme->smallPadding,
        .right = RECT_WIDTH(&contentRect) - theme->frameSize - theme->smallPadding,
        .bottom = theme->frameSize + theme->titlebarSize,
    };
}

static void window_deco_button_rect(window_t* win, element_t* elem, rect_t* rect, uint64_t index)
{
    window_deco_titlebar_rect(win, elem, rect);
    RECT_SHRINK(rect, element_get_theme(elem)->frameSize);
    uint64_t size = (rect->bottom - rect->top);
    rect->right -= size * index;
    rect->left = rect->right - size;
}

static void window_deco_draw_titlebar(window_t* win, element_t* elem, drawable_t* draw)
{
    deco_private_t* private = element_get_private(elem);
    const theme_t* theme = element_get_theme(elem);

    rect_t titlebar;
    window_deco_titlebar_rect(win, elem, &titlebar);

    draw_frame(draw, &titlebar, theme->frameSize, theme->deco.shadow, theme->deco.highlight);
    RECT_SHRINK(&titlebar, theme->frameSize);
    if (private->isFocused)
    {
        draw_gradient(draw, &titlebar, theme->deco.backgroundSelectedStart, theme->deco.backgroundSelectedEnd,
            DIRECTION_HORIZONTAL, false);
    }
    else
    {
        draw_gradient(draw, &titlebar, theme->deco.backgroundUnselectedStart, theme->deco.backgroundUnselectedEnd,
            DIRECTION_HORIZONTAL, false);
    }

    titlebar.left += theme->bigPadding;
    titlebar.right -= theme->panelSize; // Space for buttons
    draw_text(draw, &titlebar, NULL, ALIGN_MIN, ALIGN_CENTER, theme->deco.foregroundNormal, win->name);
}

static void window_deco_handle_dragging(window_t* win, element_t* elem, const event_mouse_t* event)
{
    deco_private_t* private = element_get_private(elem);

    rect_t titlebarWithoutButtons;
    window_deco_titlebar_rect(win, elem, &titlebarWithoutButtons);
    if (!(win->flags & WINDOW_NO_CONTROLS))
    {
        rect_t lastButton;
        window_deco_button_rect(win, elem, &lastButton, WINDOW_DECO_BUTTON_AMOUNT - 1);
        titlebarWithoutButtons.right = lastButton.left;
    }

    if (private->isDragging)
    {
        if (event->held & MOUSE_LEFT)
        {
            rect_t rect = RECT_INIT_DIM(event->screenPos.x - private->dragOffset.x,
                event->screenPos.y - private->dragOffset.y, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
            window_move(win, &rect);
        }
        else
        {
          private
            ->isDragging = false;
        }
    }
    else if (RECT_CONTAINS_POINT(&titlebarWithoutButtons, &event->pos) && (event->pressed & MOUSE_LEFT))
    {
      private
        ->dragOffset = (point_t){.x = event->screenPos.x - win->rect.left, .y = event->screenPos.y - win->rect.top};
      private
        ->isDragging = true;
    }
}

static uint64_t window_deco_init_controls(window_t* win, element_t* elem, deco_private_t* private)
{
    const theme_t* theme = element_get_theme(elem);
    element_t* closeButton = NULL;
    element_t* minimizeButton = NULL;

    rect_t closeRect;
    window_deco_button_rect(win, elem, &closeRect, WINDOW_DECO_CLOSE_BUTTON_INDEX);
    closeButton = button_new(elem, WINDOW_DECO_CLOSE_BUTTON_ID, &closeRect, "", ELEMENT_NO_OUTLINE);
    if (closeButton == NULL)
    {
        goto error;
    }

    rect_t minimizeRect;
    window_deco_button_rect(win, elem, &minimizeRect, WINDOW_DECO_MINIMIZE_BUTTON_INDEX);
    minimizeButton = button_new(elem, WINDOW_DECO_MINIMIZE_BUTTON_ID, &minimizeRect, "", ELEMENT_NO_OUTLINE);
    if (minimizeButton == NULL)
    {
        goto error;
    }

  private
    ->closeIcon = image_new(window_get_display(win), theme->iconClose);
    if (private->closeIcon == NULL)
    {
        goto error;
    }

  private
    ->minimizeIcon = image_new(window_get_display(win), theme->iconMinimize);
    if (private->minimizeIcon == NULL)
    {
        goto error;
    }

    element_set_image(closeButton, private->closeIcon);
    element_set_image(minimizeButton, private->minimizeIcon);

    return 0;

error:
    if (private->minimizeIcon != NULL)
    {
        image_free(private->minimizeIcon);
    }
    if (private->closeIcon != NULL)
    {
        image_free(private->closeIcon);
    }
    if (closeButton != NULL)
    {
        element_free(closeButton);
    }
    if (minimizeButton != NULL)
    {
        element_free(minimizeButton);
    }
    return ERR;
}

static uint64_t window_deco_init(window_t* win, element_t* elem)
{
    deco_private_t* private = malloc(sizeof(deco_private_t));
    if (private == NULL)
    {
        return ERR;
    }
  private
    ->isFocused = false;
  private
    ->isVisible = true;
  private
    ->isDragging = false;
  private
    ->minimizeIcon = NULL;
  private
    ->closeIcon = NULL;

    if (!(win->flags & WINDOW_NO_CONTROLS))
    {
        if (window_deco_init_controls(win, elem, private) == ERR)
        {
            free(private);
            return ERR;
        }
    }

    element_set_private(elem, private);
    return 0;
}

static void window_deco_free(element_t* elem)
{
    deco_private_t* private = element_get_private(elem);
    if (private != NULL)
    {
        if (private->closeIcon != NULL)
        {
            image_free(private->closeIcon);
        }
        if (private->minimizeIcon != NULL)
        {
            image_free(private->minimizeIcon);
        }
        free(private);
    }
}

static void window_deco_redraw(window_t* win, element_t* elem)
{
    const theme_t* theme = element_get_theme(elem);
    rect_t rect = element_get_content_rect(elem);

    drawable_t draw;
    element_draw_begin(elem, &draw);

    draw_frame(&draw, &rect, theme->frameSize, theme->deco.highlight, theme->deco.shadow);
    RECT_SHRINK(&rect, theme->frameSize);
    draw_rect(&draw, &rect, theme->deco.backgroundNormal);

    window_deco_draw_titlebar(win, elem, &draw);

    element_draw_end(elem, &draw);
}

static void window_deco_action(window_t* win, const event_lib_action_t* action)
{
    if (action->type != ACTION_RELEASE)
    {
        return;
    }

    switch (action->source)
    {
    case WINDOW_DECO_CLOSE_BUTTON_ID:
        display_push(win->disp, win->surface, EVENT_LIB_QUIT, NULL, 0);
        break;
    case WINDOW_DECO_MINIMIZE_BUTTON_ID:
        display_set_is_visible(win->disp, win->surface, false);
        break;
    }
}

static void window_deco_report(window_t* win, element_t* elem, const event_report_t* report)
{
    if (!(report->flags & REPORT_IS_FOCUSED))
    {
        return;
    }

    deco_private_t* private = element_get_private(elem);
  private
    ->isFocused = report->info.flags & SURFACE_FOCUSED;
  private
    ->isVisible = report->info.flags & SURFACE_VISIBLE;

    drawable_t draw;
    element_draw_begin(elem, &draw);
    window_deco_draw_titlebar(win, elem, &draw);
    element_draw_end(elem, &draw);
}

static uint64_t window_deco_procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_LIB_INIT:
        return window_deco_init(win, elem);

    case EVENT_LIB_DEINIT:
        window_deco_free(elem);
        break;

    case EVENT_LIB_REDRAW:
        window_deco_redraw(win, elem);
        break;

    case EVENT_LIB_ACTION:
        window_deco_action(win, &event->libAction);
        break;

    case EVENT_REPORT:
        window_deco_report(win, elem, &event->report);
        break;

    case EVENT_MOUSE:
        window_deco_handle_dragging(win, elem, &event->mouse);
        break;

    default:
        break;
    }

    return 0;
}

window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure, void* private)
{
    if (disp == NULL || name == NULL || rect == NULL || procedure == NULL || strnlen_s(name, MAX_NAME + 1) >= MAX_NAME)
    {
        errno = EINVAL;
        return NULL;
    }

    window_t* win = malloc(sizeof(window_t));
    if (win == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    list_entry_init(&win->entry);
    win->disp = disp;
    strcpy(win->name, name);
    win->rect = (rect_t){0};
    win->invalidRect = (rect_t){0};
    win->type = type;
    win->flags = flags;
    win->buffer = NULL;
    win->root = NULL;
    win->clientElement = NULL;

    const theme_t* theme = theme_global_get();
    if (flags & WINDOW_DECO)
    {
        // Expand window to fit decorations
        win->rect.left = rect->left - theme->frameSize;
        win->rect.top = rect->top - theme->frameSize - theme->titlebarSize;
        win->rect.right = rect->right + theme->frameSize;
        win->rect.bottom = rect->bottom + theme->frameSize;
    }
    else
    {
        win->rect = *rect;
    }

    cmd_surface_new_t* cmd = display_cmd_alloc(disp, CMD_SURFACE_NEW, sizeof(cmd_surface_new_t));
    if (cmd == NULL)
    {
        free(win);
        return NULL;
    }
    cmd->type = win->type;
    cmd->rect = win->rect;
    strcpy(cmd->name, win->name);
    display_cmds_flush(disp);
    event_t event;
    if (display_wait(disp, &event, EVENT_SURFACE_NEW) == ERR)
    {
        window_free(win);
        return NULL;
    }
    win->surface = event.target;

    fd_t shmem = claim(event.surfaceNew.shmemKey);
    if (shmem == ERR)
    {
        window_free(win);
        return NULL;
    }
    win->buffer =
        mmap(shmem, NULL, RECT_WIDTH(&win->rect) * RECT_HEIGHT(&win->rect) * sizeof(pixel_t), PROT_READ | PROT_WRITE);
    close(shmem);
    if (win->buffer == NULL)
    {
        window_free(win);
        return NULL;
    }

    mtx_lock(&disp->mutex);
    list_push_back(&disp->windows, &win->entry);
    mtx_unlock(&disp->mutex);

    rect_t rootRect = RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
    if (flags & WINDOW_DECO)
    {
        win->root =
            element_new_root(win, WINDOW_DECO_ELEM_ID, &rootRect, "deco", ELEMENT_NONE, window_deco_procedure, NULL);
        if (win->root == NULL)
        {
            window_free(win);
            return NULL;
        }

        rect_t clientRect = RECT_INIT(theme->frameSize, theme->frameSize + theme->titlebarSize,
            RECT_WIDTH(&win->rect) - theme->frameSize, RECT_HEIGHT(&win->rect) - theme->frameSize);

        win->clientElement =
            element_new(win->root, WINDOW_CLIENT_ELEM_ID, &clientRect, "client", ELEMENT_NONE, procedure, private);
        if (win->clientElement == NULL)
        {
            window_free(win);
            return NULL;
        }
    }
    else
    {
        win->clientElement =
            element_new_root(win, WINDOW_CLIENT_ELEM_ID, &rootRect, "client", ELEMENT_NONE, procedure, private);
        if (win->clientElement == NULL)
        {
            window_free(win);
            return NULL;
        }
        win->root = win->clientElement;
    }

    return win;
}

void window_free(window_t* win)
{
    if (win == NULL)
    {
        return;
    }

    if (win->root != NULL)
    {
        element_free(win->root);
    }

    if (win->buffer != NULL)
    {
        munmap(win->buffer, RECT_WIDTH(&win->rect) * RECT_HEIGHT(&win->rect) * sizeof(pixel_t));
    }

    cmd_surface_free_t* cmd = display_cmd_alloc(win->disp, CMD_SURFACE_FREE, sizeof(cmd_surface_free_t));
    if (cmd == NULL)
    {
        abort();
    }
    cmd->target = win->surface;
    display_cmds_flush(win->disp);

    mtx_lock(&win->disp->mutex);
    list_remove(&win->disp->windows, &win->entry);
    mtx_unlock(&win->disp->mutex);

    free(win);
}

rect_t window_get_rect(window_t* win)
{
    if (win == NULL)
    {
        return (rect_t){0};
    }

    return win->rect;
}

rect_t window_get_local_rect(window_t* win)
{
    if (win == NULL)
    {
        return (rect_t){0};
    }

    return RECT_INIT_DIM(0, 0, RECT_WIDTH(&win->rect), RECT_HEIGHT(&win->rect));
}

display_t* window_get_display(window_t* win)
{
    if (win == NULL)
    {
        return NULL;
    }

    return win->disp;
}

surface_id_t window_get_id(window_t* win)
{
    if (win == NULL)
    {
        return SURFACE_ID_NONE;
    }

    return win->surface;
}

surface_type_t window_get_type(window_t* win)
{
    if (win == NULL)
    {
        return SURFACE_NONE;
    }

    return win->type;
}

element_t* window_get_client_element(window_t* win)
{
    if (win == NULL)
    {
        return NULL;
    }

    return win->clientElement;
}

uint64_t window_move(window_t* win, const rect_t* rect)
{
    if (win == NULL || rect == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    bool hasSizeChanged = RECT_WIDTH(&win->rect) != RECT_WIDTH(rect) || RECT_HEIGHT(&win->rect) != RECT_HEIGHT(rect);
    if (hasSizeChanged && !(win->flags & WINDOW_RESIZABLE))
    {
        errno = EPERM;
        return ERR;
    }

    cmd_surface_move_t* cmd = display_cmd_alloc(win->disp, CMD_SURFACE_MOVE, sizeof(cmd_surface_move_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->target = win->surface;
    cmd->rect = *rect;
    display_cmds_flush(win->disp);
    return 0;
}

uint64_t window_set_timer(window_t* win, timer_flags_t flags, clock_t timeout)
{
    if (win == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_surface_timer_set_t* cmd = display_cmd_alloc(win->disp, CMD_SURFACE_TIMER_SET, sizeof(cmd_surface_timer_set_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->target = win->surface;
    cmd->flags = flags;
    cmd->timeout = timeout;
    display_cmds_flush(win->disp);
    return 0;
}

void window_invalidate(window_t* win, const rect_t* rect)
{
    if (win == NULL || rect == NULL)
    {
        return;
    }

    if (RECT_AREA(&win->invalidRect) == 0)
    {
        win->invalidRect = *rect;
    }
    else
    {
        RECT_EXPAND_TO_CONTAIN(&win->invalidRect, rect);
    }
}

uint64_t window_invalidate_flush(window_t* win)
{
    if (win == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (RECT_AREA(&win->invalidRect) == 0)
    {
        return 0;
    }

    cmd_surface_invalidate_t* cmd =
        display_cmd_alloc(win->disp, CMD_SURFACE_INVALIDATE, sizeof(cmd_surface_invalidate_t));
    if (cmd == NULL)
    {
        return ERR;
    }

    cmd->target = win->surface;
    cmd->invalidRect = win->invalidRect;
    display_cmds_flush(win->disp);

    win->invalidRect = (rect_t){0};
    return 0;
}

uint64_t window_dispatch(window_t* win, const event_t* event)
{
    if (win == NULL || event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (event->type)
    {
    case EVENT_LIB_REDRAW:
    {
        element_t* elem = element_find(win->root, event->libRedraw.id);
        if (elem == NULL)
        {
            return ERR;
        }

        if (element_dispatch(elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case EVENT_LIB_FORCE_ACTION:
    {
        element_t* elem = element_find(win->root, event->libForceAction.dest);
        if (elem == NULL)
        {
            return ERR;
        }

        if (element_dispatch(elem, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    case EVENT_REPORT:
    {
        if (event->report.flags & REPORT_RECT)
        {
            rect_t newRect = event->report.info.rect;

            if (RECT_WIDTH(&win->rect) != RECT_WIDTH(&newRect) || RECT_HEIGHT(&win->rect) != RECT_HEIGHT(&newRect))
            {
                event_lib_redraw_t event;
                event.id = win->root->id;
                event.shouldPropagate = true;
                display_push(win->disp, win->surface, EVENT_LIB_REDRAW, &event, sizeof(event_lib_redraw_t));
            }

            win->rect = newRect;
        }

        if (element_dispatch(win->root, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    default:
    {
        if (element_dispatch(win->root, event) == ERR)
        {
            return ERR;
        }
    }
    break;
    }

    window_invalidate_flush(win);
    return 0;
}

uint64_t window_set_focus(window_t* win)
{
    if (win == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    cmd_surface_focus_set_t* cmd = display_cmd_alloc(win->disp, CMD_SURFACE_FOCUS_SET, sizeof(cmd_surface_focus_set_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->isGlobal = false;
    cmd->target = win->surface;
    display_cmds_flush(win->disp);
    return 0;
}

uint64_t window_set_visible(window_t* win, bool isVisible)
{
    if (win == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (display_dispatch_pending(win->disp, EVENT_LIB_REDRAW, win->surface) == ERR)
    {
        return ERR;
    }

    cmd_surface_visible_set_t* cmd =
        display_cmd_alloc(win->disp, CMD_SURFACE_VISIBLE_SET, sizeof(cmd_surface_visible_set_t));
    if (cmd == NULL)
    {
        return ERR;
    }
    cmd->isGlobal = false;
    cmd->target = win->surface;
    cmd->isVisible = isVisible;
    display_cmds_flush(win->disp);
    return 0;
}
