#include "renderer.h"

#include <string.h>

#include "sysfs.h"
#include "sched.h"
#include "heap.h"
#include "utils.h"
#include "tty.h"

uint64_t framebuffer_write(File* file, const void* buffer, uint64_t count)
{
    Framebuffer* framebuffer = file->internal; 

    uint64_t pos = file->position;
    uint64_t writeCount = (pos <= framebuffer->size) ? MIN(count, framebuffer->size - pos) : 0;
    
    file->position += writeCount;
    memcpy(framebuffer->buffer + pos, buffer, writeCount);

    return writeCount;
}

void* framebuffer_mmap(File* file, void* address, uint64_t length, uint16_t flags)
{
    return NULLPTR(EIMPL);
}

void renderer_init(GopBuffer* gopBuffer)
{
    tty_start_message("Renderer initializing");

    //GOP specific
    Framebuffer* framebuffer = kmalloc(sizeof(Framebuffer));
    resource_init(&framebuffer->base, "0");
    framebuffer->base.methods.write = framebuffer_write;
    framebuffer->base.methods.mmap = framebuffer_mmap;
    framebuffer->buffer = VMM_LOWER_TO_HIGHER(gopBuffer->base);
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