#include "window.h"

#include "dwm.h"
#include "sched.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/math.h>

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
            receive->outType = MSG_NONE;
            return 0;
        }

        memcpy(receive->outData, message.data, message.size);
        receive->outType = message.type;

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

        LOCK_GUARD(&window->lock);
        window->pos.x = move->x;
        window->pos.y = move->y;

        if (window->surface.width != move->width || window->surface.height != move->height)
        {
            window->surface.width = move->width;
            window->surface.height = move->height;
            window->surface.stride = move->width;

            free(window->surface.buffer);
            window->surface.buffer = calloc(move->width * move->height, sizeof(pixel_t));
        }

        dwm_redraw();
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

    if (size != window->surface.width * window->surface.height * sizeof(pixel_t))
    {
        return ERROR(EINVAL);
    }

    if (rectIn == NULL)
    {
        memcpy(window->surface.buffer, buffer, size);
    }
    else
    {
        if (rectIn->left > rectIn->right || rectIn->top > rectIn->bottom)
        {
            return ERROR(EINVAL);
        }

        rect_t rect = (rect_t){
            .left = MAX(rectIn->left, 0),
            .top = MAX(rectIn->top, 0),
            .right = MIN(rectIn->right, window->surface.width),
            .bottom = MIN(rectIn->bottom, window->surface.height),
        };

        for (int64_t y = rect.top; y < MIN(rect.bottom, (int64_t)window->surface.height); y++)
        {
            uint64_t offset = y * sizeof(pixel_t) * window->surface.width + rect.left * sizeof(pixel_t);

            memcpy((void*)((uint64_t)window->surface.buffer + offset), (void*)((uint64_t)buffer + offset),
                (rect.right - rect.left) * sizeof(pixel_t));
        }
    }

    window->invalid = true;
    dwm_redraw();
    return 0;
}

bool window_read_avail(file_t* file)
{
    window_t* window = file->internal;

    return message_queue_avail(&window->messages);
}

window_t* window_new(const point_t* pos, uint32_t width, uint32_t height, win_type_t type)
{
    if (type > WIN_MAX)
    {
        return NULL;
    }

    window_t* window = malloc(sizeof(window_t));
    list_entry_init(&window->base);
    window->pos = *pos;
    window->surface.buffer = calloc(width * height, sizeof(pixel_t));
    window->surface.width = width;
    window->surface.height = height;
    window->surface.stride = width;
    window->type = type;
    lock_init(&window->lock);
    message_queue_init(&window->messages);
    window->invalid = true;

    return window;
}

void window_free(window_t* window)
{
    free(window->surface.buffer);
    free(window);
}

void window_populate_file(window_t* window, file_t* file, void (*cleanup)(file_t*))
{
    file->internal = window;
    file->cleanup = cleanup;
    file->ops.read_avail = window_read_avail;
    file->ops.flush = window_flush;
    file->ops.ioctl = window_ioctl;
}
