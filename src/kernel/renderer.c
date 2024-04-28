#include "renderer.h"

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "sysfs.h"
#include "sched.h"
#include "heap.h"
#include "utils.h"
#include "tty.h"

uint64_t framebuffer_ioctl(File* file, uint64_t request, void* buffer, uint64_t length)
{
    Framebuffer* framebuffer = file->internal; 

    switch (request)
    {
    case IOCTL_FB_INFO:
    {
        if (length < sizeof(ioctl_fb_info_t))
        {
            return ERROR(EBUFFER);
        }

        memcpy(buffer, &framebuffer->info, sizeof(ioctl_fb_info_t));
    }
    break;
    default:
    {
        return ERROR(EREQ);
    }
    }

    return 0;
}

void* framebuffer_mmap(File* file, void* address, uint64_t length, uint8_t prot)
{    
    Framebuffer* framebuffer = file->internal; 
    if (length > framebuffer->info.size || length == 0)
    {
        return NULLPTR(EINVAL);
    }

    return vmm_map(address, framebuffer->buffer, length, prot);
}

void renderer_init(GopBuffer* gopBuffer)
{
    tty_start_message("Renderer initializing");

    //GOP specific
    Framebuffer* framebuffer = kmalloc(sizeof(Framebuffer));
    resource_init(&framebuffer->base, "0");
    framebuffer->base.methods.ioctl = framebuffer_ioctl;
    framebuffer->base.methods.mmap = framebuffer_mmap;
    framebuffer->buffer = gopBuffer->base;
    framebuffer->info.size = gopBuffer->size;
    framebuffer->info.width = gopBuffer->width;
    framebuffer->info.height = gopBuffer->height;
    framebuffer->info.pixelsPerScanline = gopBuffer->pixelsPerScanline;
    framebuffer->info.bytesPerPixel = sizeof(uint32_t);
    framebuffer->info.blueOffset = 0 * 8;
    framebuffer->info.greenOffset = 1 * 8;
    framebuffer->info.redOffset = 2 * 8;
    framebuffer->info.alphaOffset = 3 * 8;

    sysfs_expose(&framebuffer->base, "/framebuffer");

    tty_end_message(TTY_MESSAGE_OK);
}