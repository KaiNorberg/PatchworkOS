#include "fb.h"

#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>
#include <sys/io.h>
#include <sys/win.h>

static fd_t window;
static win_info_t winInfo;
static pixel_t* buffer;

static uint8_t glyphBuffer[16 * 256];

static void fb_create(void)
{
    window = open("sys:/srv/win");
    if (window == ERR)
    {
        exit(EXIT_FAILURE);
    }

    winInfo.width = 1600;
    winInfo.height = 900;
    winInfo.x = (1920 - winInfo.width) / 2;
    winInfo.y = (1080 - winInfo.height) / 2;
    if (write(window, &winInfo, sizeof(win_info_t)))
    {
        exit(EXIT_FAILURE);
    }

    buffer = malloc(WIN_SIZE(&winInfo));
    memset(buffer, 0, WIN_SIZE(&winInfo));
    flush(window, buffer, 0, 0, winInfo.width, winInfo.height);
}

static void fb_load_font(void)
{
    fd_t fd = open("/usr/fonts/zap-vga16.psf");
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

void fb_init(void)
{
    fb_create();
    fb_load_font();
}

void fb_clear(uint32_t color)
{
    for (uint64_t i = 0; i < WIN_SIZE(&winInfo) / sizeof(pixel_t); i++)
    {
        buffer[i] = color;
    }

    flush(window, buffer, 0, 0, winInfo.width, winInfo.height);
}

void fb_scroll(uint64_t offset)
{
    offset *= winInfo.width * sizeof(pixel_t);
    memmove(buffer, (void*)(((uint64_t)buffer) + offset), WIN_SIZE(&winInfo) - offset);
    memset((void*)((uint64_t)buffer + WIN_SIZE(&winInfo) - offset), 0, offset);   
     
    flush(window, buffer, 0, 0, winInfo.width, winInfo.height);
}

void fb_char(char chr, uint64_t x, uint64_t y, uint64_t scale, uint32_t foreground, uint32_t background)
{
    char const* glyph = (char*)((uint64_t)glyphBuffer + chr * FB_CHAR_HEIGHT);
           
    for (uint32_t yOffset = 0; yOffset < FB_CHAR_HEIGHT * scale; yOffset++)
    {
        for (uint32_t xOffset = 0; xOffset < FB_CHAR_WIDTH * scale; xOffset++)
        {                
            uint32_t pixel = (*(glyph + yOffset / scale) & (0b10000000 >> (xOffset / scale))) > 0 ? foreground : background;

            *((pixel_t*)((uint64_t)buffer + (x + xOffset) * sizeof(pixel_t) + (y + yOffset) * winInfo.width * sizeof(pixel_t))) = pixel;
        }
    }   

    flush(window, buffer, x, y, FB_CHAR_WIDTH * scale, FB_CHAR_HEIGHT * scale);
}

uint64_t fb_width(void)
{
    return winInfo.width;
}

uint64_t fb_height(void)
{
    return winInfo.height;
}