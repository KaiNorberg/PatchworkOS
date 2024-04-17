#include "renderer.h"

#include "sysfs/sysfs.h"
#include "heap/heap.h"
#include "tty/tty.h"

/*
uint64_t framebuffer_read(File* file, void* buffer, uint64_t count)
{

}

void renderer_init(GopBuffer* gopBuffer)
{
    tty_start_message("Renderer initializing");

    //GOP specific
    Framebuffer* framebuffer = kmalloc(sizeof(Framebuffer));
    resource_init(&framebuffer->base);
    framebuffer->base.read = framebuffer_read;
    framebuffer->buffer = kmalloc(gopBuffer->width * gopBuffer->height);
    framebuffer->width = gopBuffer->width;
    framebuffer->height = gopBuffer->height;
    framebuffer->bytesPerPixel = sizeof(uint32_t);
    framebuffer->blueOffset = 0 * 8;
    framebuffer->greenOffset = 1 * 8;
    framebuffer->redOffset = 2 * 8;
    framebuffer->alphaOffset = 3 * 8;

    resource_expose(&framebuffer->base, "/framebuffer", "0");

    tty_end_message(TTY_MESSAGE_OK);
}
*/