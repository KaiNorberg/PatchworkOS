#include "dwm.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/dwm.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/mouse.h>

#include "lock.h"
#include "log.h"
#include "msg_queue.h"
#include "sched.h"
#include "sysfs.h"
#include "time.h"
#include "vfs.h"
#include "window.h"

static gfx_t frontbuffer;
static gfx_t backbuffer;
static rect_t screenRect;
static rect_t clientRect;

static list_t windows;
static window_t* selected;

static window_t* cursor;
static window_t* wall;

static file_t* mouse;

static lock_t lock;

static atomic_bool redrawNeeded;

static blocker_t blocker;

static void dwm_update_client_rect_unlocked(void)
{
    rect_t newRect = RECT_INIT_DIM(0, 0, backbuffer.width, backbuffer.height);

    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        window->moved = true;

        if (window->type != DWM_PANEL)
        {
            continue;
        }

        uint64_t leftDist = window->pos.x + window->gfx.width;
        uint64_t topDist = window->pos.y + window->gfx.height;
        uint64_t rightDist = backbuffer.width - window->pos.x;
        uint64_t bottomDist = backbuffer.height - window->pos.y;

        if (leftDist <= topDist && leftDist <= rightDist && leftDist <= bottomDist)
        {
            newRect.left = MAX(window->pos.x + window->gfx.width, newRect.left);
        }
        else if (topDist <= leftDist && topDist <= rightDist && topDist <= bottomDist)
        {
            newRect.top = MAX(window->pos.y + window->gfx.height, newRect.top);
        }
        else if (rightDist <= leftDist && rightDist <= topDist && rightDist <= bottomDist)
        {
            newRect.right = MIN(window->pos.x, newRect.right);
        }
        else if (bottomDist <= leftDist && bottomDist <= topDist && bottomDist <= rightDist)
        {
            newRect.bottom = MIN(window->pos.y, newRect.bottom);
        }
    }

    clientRect = newRect;
}

static void dwm_select(window_t* window)
{
    if (selected != NULL)
    {
        msg_queue_push(&selected->messages, MSG_DESELECT, NULL, 0);
    }

    if (window != NULL)
    {
        list_remove(window);
        list_push(&windows, window);
        selected = window;

        selected->moved = true;
        atomic_store(&redrawNeeded, true);

        msg_queue_push(&selected->messages, MSG_SELECT, NULL, 0);
    }
    else
    {
        selected = NULL;
    }
}

static void dwm_transfer(window_t* window, const rect_t* rect)
{
    point_t srcPoint = {
        .x = rect->left - window->pos.x,
        .y = rect->top - window->pos.y,
    };
    gfx_transfer(&backbuffer, &window->gfx, rect, &srcPoint);
}

static void dwm_redraw_others(window_t* window, const rect_t* rect)
{
    point_t srcPoint = {0};
    gfx_transfer(&backbuffer, &wall->gfx, rect, &srcPoint);

    window_t* other;
    LIST_FOR_EACH(other, &windows)
    {
        if (other == window)
        {
            continue;
        }
        LOCK_GUARD(&other->lock);

        rect_t otherRect = WINDOW_RECT(other);
        if (other->shown && RECT_OVERLAP(rect, &otherRect))
        {
            rect_t overlapRect = *rect;
            RECT_FIT(&overlapRect, &otherRect);
            RECT_FIT(&overlapRect, other->type == DWM_WINDOW ? &clientRect : &screenRect);

            dwm_transfer(other, &overlapRect);
        }
    }
}

static void dwm_invalidate_above(window_t* window, const rect_t* rect)
{
    window_t* other;
    LIST_FOR_EACH_FROM(other, window->base.next, &windows)
    {
        LOCK_GUARD(&other->lock);

        rect_t otherRect = WINDOW_RECT(other);
        if (RECT_OVERLAP(rect, &otherRect))
        {
            rect_t invalidRect = *rect;
            RECT_FIT(&invalidRect, &otherRect);
            invalidRect.left -= otherRect.left;
            invalidRect.top -= otherRect.top;
            invalidRect.right -= otherRect.left;
            invalidRect.bottom -= otherRect.top;

            other->invalid = true;
            gfx_invalidate(&other->gfx, &invalidRect);
        }
    }
}

static void dwm_swap(void)
{
    gfx_swap(&frontbuffer, &backbuffer, &backbuffer.invalidRect);
    backbuffer.invalidRect = (rect_t){0};
}

static void dwm_draw_wall(void)
{
    LOCK_GUARD(&wall->lock);
    if (!wall->invalid && !wall->moved)
    {
        return;
    }
    wall->invalid = false;
    wall->moved = false;

    rect_t wallRect = WINDOW_RECT(wall);
    RECT_FIT(&wallRect, &clientRect);
    dwm_transfer(wall, &wallRect);

    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);
        window->invalid = true;
    }
}

static void dwm_draw_windows(void)
{
    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);
        const rect_t* fitRect = window->type == DWM_WINDOW ? &clientRect : &screenRect;

        rect_t rect;
        if (window->moved)
        {
            rect = WINDOW_RECT(window);
            RECT_FIT(&rect, fitRect);

            rect_subtract_t subtract;
            RECT_SUBTRACT(&subtract, &window->prevRect, &rect);

            for (uint64_t i = 0; i < subtract.count; i++)
            {
                dwm_redraw_others(window, &window->prevRect);
            }

            window->moved = false;
            window->invalid = false;
            window->prevRect = rect;
        }
        else if (window->invalid)
        {
            rect = WINDOW_INVALID_RECT(window);
            RECT_FIT(&rect, fitRect);

            window->invalid = false;
        }
        else
        {
            continue;
        }

        dwm_transfer(window, &rect);
        dwm_invalidate_above(window, &rect);
        window->gfx.invalidRect = (rect_t){0};
        window->shown = true;
    }
}

static void dwm_draw_cursor(void)
{
    LOCK_GUARD(&cursor->lock);

    point_t srcPoint = {0};
    rect_t cursorRect = WINDOW_RECT(cursor);
    RECT_FIT(&cursorRect, &screenRect);
    gfx_transfer_blend(&backbuffer, &cursor->gfx, &cursorRect, &srcPoint);
}

static void dwm_draw_and_update_cursor(const point_t* cursorDelta)
{
    LOCK_GUARD(&cursor->lock);

    rect_t oldRect = WINDOW_RECT(cursor);
    RECT_FIT(&oldRect, &screenRect);
    dwm_redraw_others(cursor, &oldRect);

    cursor->pos.x = CLAMP(cursor->pos.x + cursorDelta->x, 0, backbuffer.width - 1);
    cursor->pos.y = CLAMP(cursor->pos.y + cursorDelta->y, 0, backbuffer.height - 1);

    rect_t cursorRect = WINDOW_RECT(cursor);
    RECT_FIT(&cursorRect, &screenRect);

    point_t srcPoint = {0};
    gfx_transfer_blend(&backbuffer, &cursor->gfx, &cursorRect, &srcPoint);

    dwm_swap();
}

static void dwm_handle_mouse_message(uint8_t buttons, const point_t* cursorDelta)
{
    static uint8_t oldButtons = 0;

    point_t oldPos = cursor->pos;
    dwm_draw_and_update_cursor(cursorDelta);

    if ((buttons & ~oldButtons) != 0) // If any button has been pressed
    {
        bool found = false;
        window_t* window;
        LIST_FOR_EACH_REVERSE(window, &windows)
        {
            LOCK_GUARD(&window->lock);

            rect_t windowRect = WINDOW_RECT(window);
            if (RECT_CONTAINS_POINT(&windowRect, cursor->pos.x, cursor->pos.y))
            {
                found = true;
                break;
            }
        }

        dwm_select(found ? window : NULL);
    }
    oldButtons = buttons;

    if (selected != NULL)
    {
        msg_mouse_t data = {
            .buttons = buttons,
            .pos.x = cursor->pos.x,
            .pos.y = cursor->pos.y,
            .deltaX = cursor->pos.x - oldPos.x,
            .deltaY = cursor->pos.y - oldPos.y,
        };
        msg_queue_push(&selected->messages, MSG_MOUSE, &data, sizeof(msg_mouse_t));
    }
}

static void dwm_poll(void)
{
    while (!atomic_load(&redrawNeeded))
    {
        sched_block_begin(&blocker);
        sched_block_do(&blocker, SEC / 1000); // TODO: Implement something better then this.
        sched_block_end(&blocker);

        uint8_t buttons = 0;
        point_t cursorDelta = {0};
        bool eventRecived = false;

        poll_file_t poll[] = {(poll_file_t){.file = mouse, .requested = POLL_READ}};
        while (vfs_poll(poll, 1, 0) != 0)
        {
            mouse_event_t event;
            LOG_ASSERT(vfs_read(mouse, &event, sizeof(mouse_event_t)), "mouse read fail");

            cursorDelta.x += event.deltaX;
            cursorDelta.y += event.deltaY;
            buttons |= event.buttons;

            eventRecived = true;
        }

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
            if (cursor != NULL)
            {
                dwm_draw_cursor();
            }
            dwm_swap();
        }
        lock_release(&lock);

        dwm_poll();
    }
}

static void dwm_window_cleanup(window_t* window)
{
    LOCK_GUARD(&lock);

    if (window == selected)
    {
        selected = NULL;
    }

    switch (window->type)
    {
    case DWM_WINDOW:
    {
        list_remove(window);
    }
    break;
    case DWM_PANEL:
    {
        list_remove(window);
        dwm_update_client_rect_unlocked();
    }
    break;
    case DWM_CURSOR:
    {
        cursor = NULL;
    }
    break;
    case DWM_WALL:
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
}

static uint64_t dwm_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    LOCK_GUARD(&lock);

    switch (request)
    {
    case IOCTL_DWM_CREATE:
    {
        if (size != sizeof(ioctl_dwm_create_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_dwm_create_t* create = argp;

        window_t* window = window_new(&create->pos, create->width, create->height, create->type, dwm_window_cleanup);
        if (window == NULL)
        {
            return ERR;
        }
        LOCK_GUARD(&window->lock);

        switch (window->type)
        {
        case DWM_WINDOW:
        {
            list_push(&windows, window);
            log_print("dwm: create window");
        }
        break;
        case DWM_PANEL:
        {
            list_push(&windows, window);
            dwm_update_client_rect_unlocked();
            log_print("dwm: create panel");
        }
        break;
        case DWM_CURSOR:
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
        case DWM_WALL:
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

        window_populate_file(window, file);

        // Preserve splash screen on boot, wall will be drawn on first flush.
        if (window->type != DWM_WALL)
        {
            dwm_redraw();
        }

        return 0;
    }
    case IOCTL_DWM_SIZE:
    {
        if (size != sizeof(ioctl_dwm_size_t))
        {
            return ERROR(EINVAL);
        }

        ioctl_dwm_size_t* size = argp;
        size->outWidth = RECT_WIDTH(&screenRect);
        size->outHeight = RECT_HEIGHT(&screenRect);

        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static file_ops_t fileOps = {
    .ioctl = dwm_ioctl,
};

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

    clientRect = RECT_INIT_GFX(&backbuffer);
    screenRect = RECT_INIT_GFX(&backbuffer);

    list_init(&windows);
    selected = NULL;

    cursor = NULL;
    wall = NULL;

    lock_init(&lock);

    mouse = vfs_open("sys:/mouse/ps2");

    atomic_init(&redrawNeeded, true);

    blocker_init(&blocker);

    sysfs_expose("/server", "dwm", &fileOps, NULL, NULL);
}

void dwm_start(void)
{
    log_print("dwm: start");

    rect_t rect = RECT_INIT_GFX(&backbuffer);
    gfx_swap(&backbuffer, &frontbuffer, &rect);

    sched_thread_spawn(dwm_loop, THREAD_PRIORITY_MAX);
}

void dwm_redraw(void)
{
    atomic_store(&redrawNeeded, true);
}

void dwm_update_client_rect(void)
{
    LOCK_GUARD(&lock);

    dwm_update_client_rect_unlocked();
}
