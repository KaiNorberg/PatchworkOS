#include "renderer.h"

#include <string.h>
#include <errno.h>

#include "sysfs.h"
#include "sched.h"
#include "heap.h"
#include "utils.h"
#include "tty.h"

void* framebuffer_mmap(File* file, void* address, uint64_t length, uint8_t prot)
{    
    Framebuffer* framebuffer = file->internal; 
    if (length > framebuffer->size || length == 0)
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
    framebuffer->base.methods.mmap = framebuffer_mmap;
    framebuffer->buffer = gopBuffer->base;
    framebuffer->size = gopBuffer->size;
    framebuffer->width = gopBuffer->width;
    framebuffer->height = gopBuffer->height;
    framebuffer->pixelsPerScanline = gopBuffer->pixelsPerScanline;
    framebuffer->bytesPerPixel = sizeof(uint32_t);
    framebuffer->blueOffset = 0 * 8;
    framebuffer->greenOffset = 1 * 8;
    framebuffer->redOffset = 2 * 8;
    framebuffer->alphaOffset = 3 * 8;

    sysfs_expose(&framebuffer->base, "/framebuffer");

    tty_end_message(TTY_MESSAGE_OK);
}