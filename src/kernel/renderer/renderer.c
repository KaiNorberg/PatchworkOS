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
    framebuffer->base = kmalloc(gopBuffer->width * gopBuffer->height);
    framebuffer->width = gopBuffer->width;
    framebuffer->height = gopBuffer->height;
    framebuffer->bytesPerPixel = sizeof(uint32_t);
    framebuffer->blueOffset = 0 * 8;
    framebuffer->greenOffset = 1 * 8;
    framebuffer->redOffset = 2 * 8;
    framebuffer->alphaOffset = 3 * 8;

    SysContext fbContext = 
    {
        .internal = framebuffer,
        .read = framebuffer_read
    };
    sysfs_register("/framebuffer", "0", &fbContext);

    tty_end_message(TTY_MESSAGE_OK);
}

void renderer_init(GopBuffer* gopBuffer)
{
    tty_start_message("Renderer initializing");

    System* system = sysfs_create_system("framebuffer");

    //GOP specific
    Framebuffer* framebuffer = kmalloc(sizeof(Framebuffer));
    framebuffer->base = kmalloc(gopBuffer->width * gopBuffer->height);
    framebuffer->width = gopBuffer->width;
    framebuffer->height = gopBuffer->height;
    framebuffer->bytesPerPixel = sizeof(uint32_t);
    framebuffer->blueOffset = 0 * 8;
    framebuffer->greenOffset = 1 * 8;
    framebuffer->redOffset = 2 * 8;
    framebuffer->alphaOffset = 3 * 8;

    SysContext fbContext = 
    {
        .internal = framebuffer,
        .read = framebuffer_read
    };
    system_create_file(system, "0", &fbContext);

    tty_end_message(TTY_MESSAGE_OK);
}*/