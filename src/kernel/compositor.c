#include "compositor.h"

#include "tty.h"
#include "sched.h"
#include "sysfs.h"
#include "utils.h"

#include <stdlib.h>
#include <errno.h>

static Lock lock;

static List windows;
static Resource compositor;

static GopBuffer frontbuffer;
static pixel_t* backbuffer;

static _Atomic(bool) redrawNeeded;

static void window_cleanup(File* file)
{
    LOCK_GUARD(&lock);

    Window* window = file->internal;
    
    list_remove(window);
    free(window->buffer);
    free(window);
}

static uint64_t window_read(File* file, void* buffer, uint64_t length)
{
    return ERROR(EIMPL);
}

static uint64_t window_flush(File* file, const void* buffer, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    Window* window = file->internal;
    LOCK_GUARD(&window->lock);

    if (x + width > frontbuffer.width || y + height > frontbuffer.height)
    {
        return ERROR(EINVAL);
    }

    for (uint64_t yOff = 0; yOff < height; yOff++) 
    {
        uint64_t offset = (y + yOff) * sizeof(pixel_t) * window->info.width + x * sizeof(pixel_t);

        memcpy((void*)((uint64_t)window->buffer + offset), (void*)((uint64_t)buffer + offset),
            width * sizeof(pixel_t));
    }

    atomic_store(&redrawNeeded, true);
    return 0;
}

static uint64_t compositor_write(File* file, const void* buffer, uint64_t length)
{
    LOCK_GUARD(&lock);

    //Check if write has already been successfully called.
    if (file->internal != NULL)
    {
        return ERROR(EACCES);
    }
 
    if (length != sizeof(win_info_t))
    {
        return ERROR(EINVAL);   
    }

    const win_info_t* info = buffer;
    if (info->width > frontbuffer.width || info->height > frontbuffer.height ||
        info->x > frontbuffer.width || info->y > frontbuffer.height ||
        info->x + info->width > frontbuffer.width || info->y + info->height > frontbuffer.height)
    {
        return ERROR(EINVAL);
    }

    Window* window = malloc(sizeof(Window));
    list_entry_init(&window->base);
    window->info = *info;
    window->buffer = malloc(WIN_SIZE(info));
    memset(window->buffer, 0, WIN_SIZE(info));
    lock_init(&window->lock);
    list_push(&windows, window);

    LOCK_GUARD(&window->lock);
    file->internal = window;
    file->cleanup = window_cleanup;
    file->methods.write = NULL;
    file->methods.read = window_read;
    file->methods.flush = window_flush;

    atomic_store(&redrawNeeded, true);
    return 0;
}

static uint64_t compositor_open(Resource* resource, File* file)
{
    file->methods.write = compositor_write;
    return 0;
}

static void compositor_draw_windows(void)
{
    LOCK_GUARD(&lock);

    Window* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);

        // Copy one line at a time from window buffer to backbuffer
        for (uint64_t y = 0; y < window->info.height; y++) 
        {
            uint64_t bufferOffset = (window->info.x * sizeof(pixel_t)) + 
                ((y + window->info.y) * sizeof(pixel_t) * frontbuffer.pixelsPerScanline);
            uint64_t windowOffset = y * sizeof(pixel_t) * window->info.width;

            memcpy((void*)((uint64_t)backbuffer + bufferOffset), 
                (void*)((uint64_t)window->buffer + windowOffset),
                window->info.width * sizeof(pixel_t));
        }
    }
}

static void compositor_loop(void)
{
    while (1)
    {
        memset(backbuffer, 0, frontbuffer.size);
        compositor_draw_windows();
        memcpy(frontbuffer.base, backbuffer, frontbuffer.size);
    
        SCHED_WAIT(atomic_load(&redrawNeeded), UINT64_MAX);
        atomic_store(&redrawNeeded, false);
    }
}

void compositor_init(GopBuffer* gopBuffer)
{
    tty_start_message("Compositor initializing");

    frontbuffer = *gopBuffer;
    frontbuffer.base = vmm_kernel_map(NULL, gopBuffer->base, gopBuffer->size);
    backbuffer = malloc(frontbuffer.size);

    list_init(&windows);
    lock_init(&lock);
    atomic_init(&redrawNeeded, true);

    resource_init(&compositor, "win", compositor_open, NULL);
    sysfs_expose(&compositor, "/srv");

    sched_thread_spawn(compositor_loop, THREAD_PRIORITY_MAX);

    tty_end_message(TTY_MESSAGE_OK);
}