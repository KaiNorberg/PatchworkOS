#include "compositor.h"

#include <errno.h>

#include "tty.h"
#include "heap.h"
#include "sched.h"
#include "sysfs.h"

#include <sys/ioctl.h>

static Lock lock;

static Resource compositor;

static File* screenbuffer;
static ioctl_fb_info_t screenInfo;

static void window_cleanup(File* file)
{
    Window* window = file->internal;
    kfree(window);
}

static uint64_t window_read(File* file, void* buffer, uint64_t length)
{
    return ERROR(EIMPL);
}

static uint64_t compositor_write(File* file, const void* buffer, uint64_t length)
{
    LOCK_GUARD(&lock);

    //Check if function has already been successfully called.
    if (file->internal != NULL)
    {
        return ERROR(EACCES);
    }

    if (length != sizeof(win_info_t))
    {
        return ERROR(EINVAL);
    }

    const win_info_t* info = buffer;
    if (info->width == 0 || info->width > screenInfo.width ||
        info->height == 0 || info->height > screenInfo.height ||
        info->x == 0 || info->x > screenInfo.width ||
        info->y == 0 || info->y > screenInfo.height)
    {
        return ERROR(EINVAL);
    }

    Window* window = kmalloc(sizeof(Window));
    window->info = *info;
    lock_init(&window->lock);

    LOCK_GUARD(&window->lock);
    file->internal = window;
    file->cleanup = window_cleanup;
    file->methods.write = NULL;
    file->methods.read = window_read;

    return 0;
}

static uint64_t compositor_open(Resource* resource, File* file)
{
    file->methods.write = compositor_write;
    return 0;
}

void compositor_init(void)
{
    tty_start_message("Compositor initializing");

    lock_init(&lock);

    screenbuffer = vfs_open("sys:/fb/0");
    if (screenbuffer == NULL)
    {
        tty_print("Failed to open screenbuffer");
        tty_end_message(TTY_MESSAGE_ER);
    }
    FILE_CALL_METHOD(screenbuffer, ioctl, IOCTL_FB_INFO, &screenInfo, sizeof(ioctl_fb_info_t));

    resource_init(&compositor, "win", compositor_open, NULL);
    sysfs_expose(&compositor, "/srv");

    tty_end_message(TTY_MESSAGE_OK);
}