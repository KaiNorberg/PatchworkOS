#include "dwm.h"

#include "lock.h"
#include "log.h"
#include "msg_queue.h"
#include "rwlock.h"
#include "sched.h"
#include "sysfs.h"
#include "systime.h"
#include "thread.h"
#include "vfs.h"
#include "window.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/dwm.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/math.h>
#include <sys/mouse.h>

static gfx_t frontbuffer;
static gfx_t backbuffer;
static rect_t screenRect;
static rect_t clientRect;

static list_t windows;
static window_t* selected;

static window_t* cursor;
static window_t* wall;

static file_t* mouse;
static file_t* keyboard;

static rwlock_t lock;

static file_t* redrawNotifier;
static atomic_bool redrawNeeded;
static wait_queue_t redrawWaitQueue;

static void dwm_update_client_rect_unlocked(void)
{
    rect_t newRect = RECT_INIT_DIM(0, 0, backbuffer.width, backbuffer.height);

    window_t* window;
    LIST_FOR_EACH(window, &windows, entry)
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
    if (selected == window)
    {
        return;
    }

    if (selected != NULL)
    {
        msg_queue_push(&selected->messages, MSG_DESELECT, NULL, 0);
    }

    if (window != NULL)
    {
        list_remove(&window->entry);
        list_push(&windows, &window->entry);
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
    point_t srcPoint = {
        .x = rect->left,
        .y = rect->top,
    };
    gfx_transfer(&backbuffer, &wall->gfx, rect, &srcPoint);

    window_t* other;
    LIST_FOR_EACH(other, &windows, entry)
    {
        if (other == window)
        {
            continue;
        }
        LOCK_DEFER(&other->lock);

        rect_t otherRect = WINDOW_RECT(other);
        if (RECT_OVERLAP(rect, &otherRect))
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
    LIST_FOR_EACH_FROM(other, window->entry.next, &windows, entry)
    {
        LOCK_DEFER(&other->lock);

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
    LOCK_DEFER(&wall->lock);
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
    LIST_FOR_EACH(window, &windows, entry)
    {
        LOCK_DEFER(&window->lock);
        window->moved = true;
    }
}

static void dwm_draw_windows(void)
{
    window_t* window;
    LIST_FOR_EACH(window, &windows, entry)
    {
        LOCK_DEFER(&window->lock);
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
    }
}

static void dwm_draw_cursor(void)
{
    LOCK_DEFER(&cursor->lock);

    point_t srcPoint = {0};
    rect_t cursorRect = WINDOW_RECT(cursor);
    RECT_FIT(&cursorRect, &screenRect);
    gfx_transfer_blend(&backbuffer, &cursor->gfx, &cursorRect, &srcPoint);
}

static window_t* dwm_window_under_point(const point_t* point)
{
    window_t* window;
    LIST_FOR_EACH_REVERSE(window, &windows, entry)
    {
        LOCK_DEFER(&window->lock);

        rect_t windowRect = WINDOW_RECT(window);
        if (RECT_CONTAINS_POINT(&windowRect, point))
        {
            return window;
        }
    }

    return NULL;
}

static void dwm_handle_mouse_message(const mouse_event_t* event)
{
    static mouse_buttons_t oldButtons = MOUSE_NONE;

    RWLOCK_READ_DEFER(&lock);
    LOCK_DEFER(&cursor->lock);

    mouse_buttons_t pressed = (event->buttons & ~oldButtons);
    mouse_buttons_t released = (oldButtons & ~event->buttons);

    point_t oldPos = cursor->pos;
    if (event->delta.x != 0 || event->delta.y != 0)
    {
        rect_t oldRect = WINDOW_RECT(cursor);
        RECT_FIT(&oldRect, &screenRect);
        dwm_redraw_others(cursor, &oldRect);

        cursor->pos.x = CLAMP(cursor->pos.x + event->delta.x, 0, backbuffer.width - 1);
        cursor->pos.y = CLAMP(cursor->pos.y + event->delta.y, 0, backbuffer.height - 1);

        point_t srcPoint = {0};
        rect_t cursorRect = WINDOW_RECT(cursor);
        RECT_FIT(&cursorRect, &screenRect);
        gfx_transfer_blend(&backbuffer, &cursor->gfx, &cursorRect, &srcPoint);

        dwm_swap();
    }

    if (pressed != MOUSE_NONE && oldButtons == MOUSE_NONE)
    {
        dwm_select(dwm_window_under_point(&cursor->pos));
    }

    if (selected != NULL)
    {
        msg_mouse_t data = {
            .held = event->buttons,
            .pressed = pressed,
            .released = released,
            .pos.x = cursor->pos.x,
            .pos.y = cursor->pos.y,
            .delta.x = cursor->pos.x - oldPos.x,
            .delta.y = cursor->pos.y - oldPos.y,
        };
        msg_queue_push(&selected->messages, MSG_MOUSE, &data, sizeof(msg_mouse_t));
    }

    oldButtons = event->buttons;
}

static void dwm_poll_mouse(void)
{
    if (cursor == NULL)
    {
        return;
    }

    mouse_event_t total = {0};
    bool received = false;
    while (1)
    {
        poll_file_t poll = {.file = mouse, .requested = POLL_READ};
        vfs_poll(&poll, 1, 0);
        if (poll.occurred & POLL_READ)
        {
            mouse_event_t event;
            ASSERT_PANIC(vfs_read(mouse, &event, sizeof(mouse_event_t)) == sizeof(mouse_event_t));

            total.buttons |= event.buttons;
            total.delta.x += event.delta.x;
            total.delta.y += event.delta.y;
            received = true;
        }
        else
        {
            break;
        }
    }

    if (received)
    {
        dwm_handle_mouse_message(&total);
    }
}

static void dwm_poll_keyboard(void)
{
    poll_file_t poll = {.file = keyboard, .requested = POLL_READ};
    vfs_poll(&poll, 1, 0);
    if (poll.occurred & POLL_READ)
    {
        kbd_event_t event;
        ASSERT_PANIC(vfs_read(keyboard, &event, sizeof(kbd_event_t)) == sizeof(kbd_event_t));

        if (selected != NULL)
        {
            msg_kbd_t data = {.code = event.code, .type = event.type, .mods = event.mods};
            msg_queue_push(&selected->messages, MSG_KBD, &data, sizeof(msg_kbd_t));
        }
    }
}

static void dwm_poll(void)
{
    while (!atomic_exchange_explicit(&redrawNeeded, false, __ATOMIC_RELAXED))
    {
        poll_file_t poll[] = {{.file = mouse, .requested = POLL_READ}, {.file = keyboard, .requested = POLL_READ},
            {.file = redrawNotifier, .requested = POLL_READ}};
        vfs_poll(poll, 3, NEVER);

        dwm_poll_mouse();
        dwm_poll_keyboard();
    }
}
static void dwm_loop(void)
{
    while (1)
    {
        rwlock_read_acquire(&lock);
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
        rwlock_read_release(&lock);

        dwm_poll();
    }
}

static uint64_t dwm_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    RWLOCK_WRITE_DEFER(&lock);

    switch (request)
    {
    case IOCTL_DWM_CREATE:
    {
        if (size != sizeof(ioctl_dwm_create_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_dwm_create_t* create = argp;

        window_t* window = window_new(&create->pos, create->width, create->height, create->type);
        if (window == NULL)
        {
            return ERR;
        }
        LOCK_DEFER(&window->lock);

        switch (window->type)
        {
        case DWM_WINDOW:
        {
            list_push(&windows, &window->entry);
            dwm_select(window);
            printf("dwm: create type=window");
        }
        break;
        case DWM_PANEL:
        {
            list_push(&windows, &window->entry);
            dwm_update_client_rect_unlocked();
            dwm_select(window);
            printf("dwm: create type=panel");
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
            printf("dwm: create type=cursor");
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
            printf("dwm: create type=wall");
        }
        break;
        default:
        {
            log_panic(NULL, "Invalid window type %d", window->type);
        }
        }

        window_populate_file(window, file);
        dwm_redraw();
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

SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(dwm_open, &fileOps);

static void dwm_cleanup(sysobj_t* sysobj, file_t* file)
{
    RWLOCK_WRITE_DEFER(&lock);

    window_t* window = file->private;
    if (window == NULL)
    {
        return;
    }

    if (window == selected)
    {
        selected = NULL;
    }

    switch (window->type)
    {
    case DWM_WINDOW:
    {
        printf("dwm: cleanup type=window");
        list_remove(&window->entry);
        window_free(window);
    }
    break;
    case DWM_PANEL:
    {
        printf("dwm: cleanup type=panel");
        list_remove(&window->entry);
        window_free(window);
        dwm_update_client_rect_unlocked();
    }
    break;
    case DWM_CURSOR:
    {
        printf("dwm: cleanup type=cursor");
        cursor = NULL;
        window_free(window);
    }
    break;
    case DWM_WALL:
    {
        printf("dwm: cleanup type=wall");
        wall = NULL;
        window_free(window);
    }
    break;
    default:
    {
        log_panic(NULL, "Invalid window type %d", window->type);
    }
    }

    if (wall != NULL)
    {
        wall->moved = true;
    }

    dwm_redraw();
}

static sysobj_ops_t resOps = {
    .open = dwm_open,
    .cleanup = dwm_cleanup,
};

static wait_queue_t* dwm_redraw_notifier_status(file_t* file, poll_file_t* pollFile)
{
    pollFile->occurred = POLL_READ & atomic_load(&redrawNeeded);
    return &redrawWaitQueue;
}

static file_ops_t redrawNotifierOps = {
    .poll = dwm_redraw_notifier_status,
};

void dwm_init(gop_buffer_t* gopBuffer)
{
    printf("dwm: init width=%d height=%d", (uint64_t)gopBuffer->width, (uint64_t)gopBuffer->height);

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

    rwlock_init(&lock);

    // TODO: Add system to choose input devices
    mouse = vfs_open("sys:/mouse/ps2");
    keyboard = vfs_open("sys:/kbd/ps2");

    redrawNotifier = file_new(NULL);
    redrawNotifier->ops = &redrawNotifierOps;
    atomic_init(&redrawNeeded, true);
    wait_queue_init(&redrawWaitQueue);

    sysobj_new("/", "dwm", &resOps, NULL);
}

void dwm_start(void)
{
    rect_t rect = RECT_INIT_GFX(&backbuffer);
    gfx_swap(&backbuffer, &frontbuffer, &rect);

    thread_t* thread = thread_new(sched_process(), dwm_loop, PRIORITY_MAX - 1);
    ASSERT_PANIC(thread != NULL);
    sched_push(thread);
    printf("dwm: start pid=%d tid=%d", thread->process->id, thread->id);
}

void dwm_redraw(void)
{
    atomic_store(&redrawNeeded, true);
    waitsys_unblock(&redrawWaitQueue, WAITSYS_ALL);
}

void dwm_update_client_rect(void)
{
    RWLOCK_WRITE_DEFER(&lock);

    dwm_update_client_rect_unlocked();
}
