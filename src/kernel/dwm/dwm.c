#include "dwm.h"

#include "_AUX/rect_t.h"
#include "config.h"
#include "list.h"
#include "lock.h"
#include "log.h"
#include "message.h"
#include "sched.h"
#include "sys/win.h"
#include "sysfs.h"
#include "time.h"
#include "vfs.h"
#include "window.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/mouse.h>

static resource_t dwm;

static surface_t frontbuffer;
static surface_t backbuffer;
static rect_t screenArea;
static rect_t clientArea;

static list_t windows;
static list_t panels;

static window_t* cursor;
static window_t* wall;
static window_t* selected;

static file_t* mouse;

static lock_t lock;

static _Atomic(bool) redrawNeeded;

static void dwm_update_client_area(void)
{
    rect_t newArea = RECT_INIT_DIM(0, 0, backbuffer.width, backbuffer.height);

    window_t* panel;
    LIST_FOR_EACH(panel, &panels)
    {
        uint64_t leftDist = panel->pos.x + panel->surface.width;
        uint64_t topDist = panel->pos.y + panel->surface.height;
        uint64_t rightDist = backbuffer.width - panel->pos.x;
        uint64_t bottomDist = backbuffer.height - panel->pos.y;

        if (leftDist <= topDist && leftDist <= rightDist && leftDist <= bottomDist)
        {
            newArea.left = MAX(panel->pos.x + panel->surface.width, newArea.left);
        }
        else if (topDist <= leftDist && topDist <= rightDist && topDist <= bottomDist)
        {
            newArea.top = MAX(panel->pos.y + panel->surface.height, newArea.top);
        }
        else if (rightDist <= leftDist && rightDist <= topDist && rightDist <= bottomDist)
        {
            newArea.right = MIN(panel->pos.x, newArea.right);
        }
        else if (bottomDist <= leftDist && bottomDist <= topDist && bottomDist <= rightDist)
        {
            newArea.bottom = MIN(panel->pos.y, newArea.bottom);
        }
    }

    clientArea = newArea;
}

static void dwm_window_rect(window_t* window, rect_t* rect)
{
    *rect = RECT_INIT_DIM(window->pos.x, window->pos.y, window->surface.width, window->surface.height);
}

static void dwm_window_invalid_rect(window_t* window, rect_t* rect)
{
    *rect = RECT_INIT_DIM(window->pos.x + window->surface.invalidArea.left, window->pos.y + window->surface.invalidArea.top,
        RECT_WIDTH(&window->surface.invalidArea), RECT_HEIGHT(&window->surface.invalidArea));
}

static void dwm_window_transfer(window_t* window, const rect_t* rect)
{
    point_t srcPoint = {
        .x = rect->left - window->pos.x,
        .y = rect->top - window->pos.y,
    };
    gfx_transfer(&backbuffer, &window->surface, rect, &srcPoint);
}

static void dwm_window_select(window_t* window)
{
    if (selected != NULL)
    {
        message_queue_push(&selected->messages, MSG_DESELECT, NULL, 0);
    }

    if (window != NULL)
    {
        list_remove(window);
        list_push(&windows, window);
        selected = window;

        message_queue_push(&selected->messages, MSG_SELECT, NULL, 0);
    }
    else
    {
        selected = NULL;
    }
}

static void dwm_window_cleanup(file_t* file)
{
    LOCK_GUARD(&lock);

    window_t* window = file->internal;

    if (window == selected)
    {
        selected = NULL;
    }

    switch (window->type)
    {
    case WIN_WINDOW:
    {
        list_remove(window);
    }
    break;
    case WIN_PANEL:
    {
        list_remove(window);

        dwm_update_client_area();
    }
    break;
    case WIN_CURSOR:
    {
        cursor = NULL;
    }
    break;
    case WIN_WALL:
    {
        wall = NULL;
    }
    break;
    default:
    {
        log_panic(NULL, "Invalid window type %d", window->type);
    }
    }

    if (wall != NULL)
    {
        wall->invalid = true;
    }

    window_free(window);
}

static uint64_t dwm_ioctl(file_t* file, uint64_t request, void* buffer, uint64_t length)
{
    LOCK_GUARD(&lock);

    switch (request)
    {
    case IOCTL_DWM_CREATE:
    {
        if (length != sizeof(ioctl_dwm_create_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_dwm_create_t* create = buffer;

        window_t* window = window_new(&create->pos, create->width, create->height, create->type);
        if (window == NULL)
        {
            return ERR;
        }

        LOCK_GUARD(&window->lock);

        switch (window->type)
        {
        case WIN_WINDOW:
        {
            list_push(&windows, window);
            log_print("dwm: create window");
        }
        break;
        case WIN_PANEL:
        {
            list_push(&panels, window);

            dwm_update_client_area();
            log_print("dwm: create panel");
        }
        break;
        case WIN_CURSOR:
        {
            if (cursor != NULL)
            {
                window_free(window);
                return ERROR(EEXIST);
            }

            cursor = window;
            log_print("dwm: create cursor");
        }
        break;
        case WIN_WALL:
        {
            if (wall != NULL)
            {
                window_free(window);
                return ERROR(EEXIST);
            }

            wall = window;
            log_print("dwm: create wall");
        }
        break;
        default:
        {
            log_panic(NULL, "Invalid window type %d", window->type);
        }
        }

        window_populate_file(window, file, dwm_window_cleanup);

        // Preserve splash screen on boot, wall will be drawn on first flush.
        if (window->type != WIN_WALL)
        {
            dwm_redraw();
        }
        return 0;
    }
    case IOCTL_DWM_SIZE:
    {
        if (length != sizeof(ioctl_dwm_size_t))
        {
            return ERROR(EINVAL);
        }

        ioctl_dwm_size_t* size = buffer;
        size->outWidth = RECT_WIDTH(&screenArea);
        size->outHeight = RECT_HEIGHT(&screenArea);

        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t dwm_open(resource_t* resource, file_t* file)
{
    file->ops.ioctl = dwm_ioctl;
    return 0;
}

static void dwm_redraw_below(window_t* window, const rect_t* rect)
{
    point_t srcPoint = {0};
    gfx_transfer(&backbuffer, &wall->surface, rect, &srcPoint);

    window_t* other;
    LIST_FOR_EACH(other, &windows)
    {
        if (other == window)
        {
            continue;
        }
        LOCK_GUARD(&other->lock);

        rect_t otherRect;
        dwm_window_rect(other, &otherRect);

        if (RECT_OVERLAP(rect, &otherRect))
        {
            rect_t invalidRect = *rect;
            RECT_FIT(&invalidRect, &otherRect);

            dwm_window_transfer(other, &invalidRect);
        }
    }
}

static void dwm_invalidate_above(window_t* window, const rect_t* rect)
{
    window_t* other;
    LIST_FOR_EACH_FROM(other, window->base.next, &windows)
    {
        LOCK_GUARD(&other->lock);

        rect_t otherRect;
        dwm_window_rect(other, &otherRect);

        if (RECT_OVERLAP(rect, &otherRect))
        {
            rect_t invalidRect = *rect;
            RECT_FIT(&invalidRect, &otherRect);
            invalidRect.left -= otherRect.left;
            invalidRect.top -= otherRect.top;
            invalidRect.right -= otherRect.left;
            invalidRect.bottom -= otherRect.top;

            other->invalid = true;
            gfx_invalidate(&other->surface, &invalidRect);
        }
    }
}

static void dwm_draw_windows(void)
{
    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);

        rect_t rect;
        if (window->moved)
        {
            dwm_window_rect(window, &rect);
            RECT_FIT(&rect, &clientArea);

            rect_subtract_t subtract;
            RECT_SUBTRACT(&subtract, &window->prevRect, &rect);

            for (uint64_t i = 0; i < subtract.count; i++)
            {
                dwm_redraw_below(window, &window->prevRect);
            }

            dwm_window_transfer(window, &rect);

            window->moved = false;
            window->invalid = false;
            window->prevRect = rect;
        }
        else if (window->invalid)
        {
            dwm_window_invalid_rect(window, &rect);
            RECT_FIT(&rect, &clientArea);

            window->invalid = false;
        }
        else
        {
            continue;
        }

        dwm_window_transfer(window, &rect);
        dwm_invalidate_above(window, &rect);
        window->surface.invalidArea = (rect_t){0};
    }
}

static void dwm_draw_wall(void)
{
    LOCK_GUARD(&wall->lock);

    if (!wall->invalid)
    {
        return;
    }
    wall->invalid = false;

    rect_t wallRect;
    dwm_window_rect(wall, &wallRect);
    RECT_FIT(&wallRect, &clientArea);

    point_t srcPoint = {.x = clientArea.left, .y = clientArea.top};
    gfx_transfer(&backbuffer, &wall->surface, &wallRect, &srcPoint);

    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        window->invalid = true;
    }
}

static void dwm_draw_panels(void)
{
    window_t* panel;
    LIST_FOR_EACH(panel, &panels)
    {
        LOCK_GUARD(&panel->lock);

        if (!panel->invalid)
        {
            continue;
        }
        panel->invalid = false;

        rect_t panelRect = RECT_INIT_DIM(panel->pos.x, panel->pos.y, panel->surface.width, panel->surface.height);

        if (!RECT_CONTAINS(&screenArea, &panelRect))
        {
            continue;
        }

        point_t srcPoint = {0};
        gfx_transfer(&backbuffer, &panel->surface, &panelRect, &srcPoint);
    }
}

static void dwm_draw_cursor(const point_t* oldPos)
{
    LOCK_GUARD(&cursor->lock);

    if (oldPos != NULL)
    {
        rect_t oldRect = RECT_INIT_DIM(oldPos->x, oldPos->y, cursor->surface.width, cursor->surface.height);
        RECT_FIT(&oldRect, &screenArea);
        gfx_transfer(&frontbuffer, &backbuffer, &oldRect, oldPos);
    }

    rect_t cursorRect = RECT_INIT_DIM(cursor->pos.x, cursor->pos.y, cursor->surface.width, cursor->surface.height);
    RECT_FIT(&cursorRect, &screenArea);

    point_t srcPoint = {0};
    gfx_transfer_blend(&frontbuffer, &cursor->surface, &cursorRect, &srcPoint);
}

static void dwm_handle_mouse_message(uint8_t buttons, const point_t* cursorDelta)
{
    static uint8_t oldButtons = 0;

    point_t oldPos = cursor->pos;

    cursor->pos.x = CLAMP(cursor->pos.x + cursorDelta->x, 0, backbuffer.width - 1);
    cursor->pos.y = CLAMP(cursor->pos.y + cursorDelta->y, 0, backbuffer.height - 1);

    dwm_draw_cursor(&oldPos);

    if (buttons != oldButtons)
    {
        window_t* window;
        LIST_FOR_EACH_REVERSE(window, &windows)
        {
            LOCK_GUARD(&window->lock);

            rect_t windowRect = RECT_INIT_DIM(window->pos.x, window->pos.y, window->surface.width, window->surface.height);

            if (RECT_CONTAINS_POINT(&windowRect, cursor->pos.x, cursor->pos.y))
            {
                dwm_window_select(window);
                goto found;
            }
        }

        dwm_window_select(NULL);
    }
found:
    oldButtons = buttons;

    if (selected != NULL)
    {
        msg_mouse_t message = {
            .time = time_uptime(),
            .buttons = buttons,
            .pos.x = cursor->pos.x,
            .pos.y = cursor->pos.y,
            .deltaX = cursor->pos.x - oldPos.x,
            .deltaY = cursor->pos.y - oldPos.y,
        };
        message_queue_push(&selected->messages, MSG_MOUSE, &message, sizeof(msg_mouse_t));
    }
}

static void dwm_poll(void)
{
    static nsec_t before = 0;

    while (!atomic_load(&redrawNeeded))
    {
        uint8_t buttons = 0;
        point_t cursorDelta = {0};
        bool eventRecived = false;

        do
        {
            poll_file_t poll[] = {(poll_file_t){.file = mouse, .requested = POLL_READ}};
            while (vfs_poll(poll, 1, 0) != 0)
            {
                mouse_event_t event;
                FILE_CALL(mouse, read, &event, sizeof(mouse_event_t));

                cursorDelta.x += event.deltaX;
                cursorDelta.y += event.deltaY;
                buttons |= event.buttons;

                eventRecived = true;
            }

            sched_yield();
        } while (time_uptime() - before < SEC / CONFIG_DWM_FPS);
        before = time_uptime();

        lock_acquire(&lock);
        if (cursor != NULL && eventRecived)
        {
            dwm_handle_mouse_message(buttons, &cursorDelta);
        }
        lock_release(&lock);
    }
    atomic_store(&redrawNeeded, false);
}

static void dwm_loop(void)
{
    while (1)
    {
        lock_acquire(&lock);
        if (wall != NULL)
        {
            dwm_draw_wall();

            dwm_draw_windows();
            dwm_draw_panels();

            gfx_swap(&frontbuffer, &backbuffer, &backbuffer.invalidArea);
            backbuffer.invalidArea = (rect_t){0};
        }

        if (cursor != NULL)
        {
            dwm_draw_cursor(NULL);
        }
        lock_release(&lock);

        dwm_poll();
    }
}

void dwm_init(gop_buffer_t* gopBuffer)
{
    log_print("dwm: %dx%d", (uint64_t)gopBuffer->width, (uint64_t)gopBuffer->height);

    frontbuffer.buffer = gopBuffer->base;
    frontbuffer.height = gopBuffer->height;
    frontbuffer.width = gopBuffer->width;
    frontbuffer.stride = gopBuffer->stride;

    backbuffer.buffer = malloc(gopBuffer->size);
    backbuffer.height = gopBuffer->height;
    backbuffer.width = gopBuffer->width;
    backbuffer.stride = gopBuffer->stride;

    clientArea = RECT_INIT_SURFACE(&backbuffer);
    screenArea = RECT_INIT_SURFACE(&backbuffer);

    list_init(&windows);
    list_init(&panels);
    cursor = NULL;
    wall = NULL;

    lock_init(&lock);

    mouse = vfs_open("sys:/mouse/ps2");

    atomic_init(&redrawNeeded, true);

    resource_init(&dwm, "dwm", dwm_open, NULL);
    sysfs_expose(&dwm, "/server");
}

void dwm_start(void)
{
    log_print("dwm: start");

    rect_t rect = RECT_INIT_SURFACE(&backbuffer);
    gfx_swap(&backbuffer, &frontbuffer, &rect);

    sched_thread_spawn(dwm_loop, THREAD_PRIORITY_MAX);
}

void dwm_redraw(void)
{
    atomic_store(&redrawNeeded, true);
}
