#include "fb.h"

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mem.h>
#include <sys/io.h>

static ioctl_fb_info_t fbInfo;

//TODO: Implement malloc and replace this.
static uint8_t glyphBuffer[16 * 256];

static void fb_map()
{
    fd_t fd = open("A:/framebuffer/0");
    if (fd == ERR)
    {
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, IOCTL_FB_INFO, &fbInfo, sizeof(ioctl_fb_info_t)) == ERR)
    {
        exit(EXIT_FAILURE);
    }

    if (mmap(fd, FB_ADDR, fbInfo.size, PROT_READ | PROT_WRITE) == NULL)
    {
        exit(EXIT_FAILURE);
    }

    close(fd);
}

static void fb_load_font()
{
    fd_t fd = open("/fonts/zap-vga16.psf");
    if (fd == ERR)
    {
        exit(EXIT_FAILURE);
    }

    PsfHeader header;
    if (read(fd, &header, sizeof(PsfHeader)) != sizeof(PsfHeader))
    {
        exit(EXIT_FAILURE);
    }

    if (header.magic != PSF_MAGIC)
    {
        exit(EXIT_FAILURE);
    }

    seek(fd, sizeof(PsfHeader), SEEK_SET);
    read(fd, glyphBuffer, 16 * 256);

    close(fd);
}

void fb_init()
{
    fb_map();
    fb_load_font();
}

void fb_clear(uint32_t color)
{
    for (uint64_t i = 0; i < fbInfo.size / sizeof(uint32_t); i++)
    {
        ((uint32_t*)FB_ADDR)[i] = color;
    }
}

void fb_scroll(uint64_t offset)
{
    offset *= fbInfo.pixelsPerScanline * sizeof(uint32_t);
    memmove(FB_ADDR, (void*)(((uint64_t)FB_ADDR) + offset), fbInfo.size - offset);
    memset((void*)(((uint64_t)FB_ADDR + fbInfo.size - offset)), 0, offset);
}

void fb_char(char chr, uint64_t x, uint64_t y, uint64_t scale, uint32_t foreground, uint32_t background)
{
    char* glyph = (char*)((uint64_t)glyphBuffer + chr * FB_CHAR_HEIGHT);
           
    for (uint32_t yOffset = 0; yOffset < FB_CHAR_HEIGHT * scale; yOffset++)
    {
        for (uint32_t xOffset = 0; xOffset < FB_CHAR_WIDTH * scale; xOffset++)
        {                
            uint32_t pixel = (*(glyph + yOffset / scale) & (0b10000000 >> (xOffset / scale))) > 0 ? foreground : background;

            *((uint32_t *)(FB_ADDR + (x + xOffset) * sizeof(uint32_t) + (y + yOffset) * fbInfo.pixelsPerScanline * sizeof(uint32_t))) = pixel;
        }
    }
}

uint64_t fb_width()
{
    return fbInfo.width;
}

uint64_t fb_height()
{
    return fbInfo.height;
}