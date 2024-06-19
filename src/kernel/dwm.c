#include "dwm.h"

#include "_AUX/rect_t.h"
#include "list.h"
#include "lock.h"
#include "sched.h"
#include "splash.h"
#include "sys/win.h"
#include "sysfs.h"

#include <errno.h>
#include <stdlib.h>

static lock_t lock;

static list_t windows;
static resource_t dwm;

static gop_buffer_t frontbuffer;
static surface_t backbuffer;
static win_theme_t theme;

static _Atomic(bool) redrawNeeded;

static void message_queue_init(message_queue_t* queue)
{
    memset(queue->queue, 0, sizeof(queue->queue));
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

static void message_queue_push(message_queue_t* queue, msg_t type, const void* data, uint64_t size)
{
    LOCK_GUARD(&queue->lock);

    message_t message = (message_t){
        .size = size,
        .type = type,
    };
    if (data != NULL)
    {
        memcpy(queue->queue[queue->writeIndex].data, data, size);
    }

    queue->queue[queue->writeIndex] = message;
    queue->writeIndex = (queue->writeIndex + 1) % MESSAGE_QUEUE_MAX;
}

static bool message_queue_pop(message_queue_t* queue, message_t* out)
{
    LOCK_GUARD(&queue->lock);

    if (queue->readIndex == queue->writeIndex)
    {
        return false;
    }

    *out = queue->queue[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % MESSAGE_QUEUE_MAX;

    return true;
}

static void window_cleanup(file_t* file)
{
    window_t* window = file->internal;

    list_remove(window);
    free(window->buffer);
    free(window);
}

static uint64_t window_ioctl(file_t* file, uint64_t request, void* buffer, uint64_t length)
{
    window_t* window = file->internal;

    switch (request)
    {
    case IOCTL_WIN_RECEIVE:
    {
        if (length != sizeof(ioctl_win_receive_t))
        {
            return ERROR(EINVAL);
        }
        ioctl_win_receive_t* receive = buffer;

        message_t message;
        if (SCHED_WAIT(message_queue_pop(&window->messages, &message), receive->timeout) == SCHED_WAIT_TIMEOUT)
        {
            receive->type = MSG_NONE;
            return 0;
        }

        memcpy(receive->data, message.data, message.size);
        receive->type = message.type;

        return 0;
    }
    case IOCTL_WIN_SEND:
    {
        if (length != sizeof(ioctl_win_send_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_win_send_t* send = buffer;

        message_queue_push(&window->messages, send->type, send->data, MSG_MAX_DATA);

        return 0;
    }
    case IOCTL_WIN_MOVE:
    {
        if (length != sizeof(ioctl_win_move_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_win_move_t* move = buffer;

        if (move->width > frontbuffer.width || move->height > frontbuffer.height || move->x > frontbuffer.width ||
            move->y > frontbuffer.height || move->x + move->width > frontbuffer.width ||
            move->y + move->height > frontbuffer.height)
        {
            return ERROR(EINVAL);
        }

        LOCK_GUARD(&window->lock);
        window->x = move->x;
        window->y = move->y;

        if (window->width != move->width || window->height != move->height)
        {
            window->width = move->width;
            window->height = move->height;

            free(window->buffer);
            window->buffer = calloc(window->width * window->height, sizeof(pixel_t));
        }

        atomic_store(&redrawNeeded, true);
        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t window_flush(file_t* file, const void* buffer, uint64_t size, const rect_t* rectIn)
{
    window_t* window = file->internal;
    LOCK_GUARD(&window->lock);

    if (size != window->width * window->height * sizeof(pixel_t))
    {
        return ERROR(EINVAL);
    }

    if (rectIn == NULL)
    {
        memcpy(window->buffer, buffer, size);
    }
    else
    {
        volatile rect_t rect = *rectIn;
        if (rect.right > frontbuffer.width || rect.bottom > frontbuffer.height || rect.left > frontbuffer.width ||
            rect.top > frontbuffer.height)
        {
            return ERROR(EINVAL);
        }

        for (uint64_t y = rect.top; y < rect.bottom; y++)
        {
            uint64_t offset = y * sizeof(pixel_t) * window->width + rect.left * sizeof(pixel_t);

            memcpy((void*)((uint64_t)window->buffer + offset), (void*)((uint64_t)buffer + offset),
                (rect.right - rect.left) * sizeof(pixel_t));
        }
    }

    atomic_store(&redrawNeeded, true);
    return 0;
}

static uint64_t dwm_ioctl(file_t* file, uint64_t request, void* buffer, uint64_t length)
{
    LOCK_GUARD(&lock);

    switch (request)
    {
    case IOCTL_DWM_CREATE:
    {
        // Check if write has already been successfully called.
        if (file->internal != NULL)
        {
            return ERROR(EBUSY);
        }

        if (length != sizeof(ioctl_dwm_create_t))
        {
            return ERROR(EINVAL);
        }

        const ioctl_dwm_create_t* create = buffer;
        if (create->width > frontbuffer.width || create->height > frontbuffer.height || create->x > frontbuffer.width ||
            create->y > frontbuffer.height || create->x + create->width > frontbuffer.width ||
            create->y + create->height > frontbuffer.height)
        {
            return ERROR(EINVAL);
        }

        window_t* window = malloc(sizeof(window_t));
        list_entry_init(&window->base);
        window->x = create->x;
        window->y = create->y;
        window->width = create->width;
        window->height = create->height;
        window->buffer = calloc(create->width * create->height, sizeof(pixel_t));
        lock_init(&window->lock);
        message_queue_init(&window->messages);
        LOCK_GUARD(&window->lock);

        file->internal = window;
        file->cleanup = window_cleanup;
        file->ops.flush = window_flush;
        file->ops.ioctl = window_ioctl;

        list_push(&windows, window);
        atomic_store(&redrawNeeded, true);
        return 0;
    }
    case IOCTL_DWM_SIZE:
    {
        if (length != sizeof(ioctl_dwm_size_t))
        {
            return ERROR(EINVAL);
        }

        ioctl_dwm_size_t* size = buffer;
        size->width = backbuffer.width;
        size->height = backbuffer.height;

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

static void dwm_draw_windows(void)
{
    // TODO: Optimize this, add rectangle subtraction.

    LOCK_GUARD(&lock);

    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);

        // Copy one line at a time from window buffer to backbuffer
        for (uint64_t y = 0; y < window->height; y++)
        {
            uint64_t bufferOffset = (window->x * sizeof(pixel_t)) + (y + window->y) * sizeof(pixel_t) * frontbuffer.stride;
            uint64_t windowOffset = y * sizeof(pixel_t) * window->width;

            memcpy((void*)((uint64_t)backbuffer.buffer + bufferOffset), (void*)((uint64_t)window->buffer + windowOffset),
                window->width * sizeof(pixel_t));
        }
    }
}

static void dwm_loop(void)
{
    while (1)
    {
        rect_t rect;
        RECT_INIT_DIM(&rect, 0, 0, backbuffer.width, backbuffer.height);
        gfx_rect(&backbuffer, &rect, theme.wall);

        dwm_draw_windows();
        memcpy(frontbuffer.base, backbuffer.buffer, frontbuffer.size);

        SCHED_WAIT(atomic_load(&redrawNeeded), NEVER);
        atomic_store(&redrawNeeded, false);
    }
}

void dwm_init(gop_buffer_t* gopBuffer)
{
    frontbuffer = *gopBuffer;
    frontbuffer.base = gopBuffer->base;
    backbuffer.buffer = malloc(frontbuffer.size);
    backbuffer.height = frontbuffer.height;
    backbuffer.width = frontbuffer.width;
    backbuffer.stride = frontbuffer.stride;

    list_init(&windows);
    lock_init(&lock);
    atomic_init(&redrawNeeded, true);
    win_default_theme(&theme);

    resource_init(&dwm, "dwm", dwm_open, NULL);
    sysfs_expose(&dwm, "/srv");
}

void dwm_start(void)
{
    sched_thread_spawn(dwm_loop, THREAD_PRIORITY_MAX);
}
