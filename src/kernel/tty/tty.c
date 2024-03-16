#include "tty.h"

#include <libc/string.h>
#include <libc/stdarg.h>

#include "utils/utils.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "heap/heap.h"

#include <common/boot_info/boot_info.h>

static GopBuffer frontbuffer;
static PsfFont font;

static uint32_t column;
static uint32_t row;

static Pixel background;
static Pixel foreground;

static uint8_t scale;

static Lock lock;

void tty_init(GopBuffer* gopBuffer, PsfFont* screenFont)
{
    frontbuffer.base = vmm_map(gopBuffer->base, SIZE_IN_PAGES(gopBuffer->size), PAGE_FLAG_WRITE);
    frontbuffer.size = gopBuffer->size;
    frontbuffer.width = gopBuffer->width;
    frontbuffer.height = gopBuffer->height;
    frontbuffer.pixelsPerScanline = gopBuffer->pixelsPerScanline;
    
    font.header = screenFont->header;   
    font.glyphs = kmalloc(screenFont->glyphsSize);
    memcpy(font.glyphs, screenFont->glyphs, screenFont->glyphsSize);

    scale = 1;
    column = 0;
    row = 0;

    background.a = 0;
    background.r = 0;
    background.g = 0;
    background.b = 0;
    
    foreground.a = 255;
    foreground.r = 255;
    foreground.g = 255;
    foreground.b = 255;
        
    lock = lock_create();

    tty_clear();
}

void tty_set_scale(uint8_t value)
{
    scale = value;
}

void tty_set_foreground(Pixel value)
{
    foreground = value;
}

void tty_set_background(Pixel value)
{
    background = value;
}

void tty_set_pos(uint32_t x, uint32_t y)
{
    column = x;   
    row = y;
}

void tty_set_row(uint32_t value)
{
    row = value;
}

uint32_t tty_get_row(void)
{
    return row;
}

void tty_set_column(uint32_t value)
{
    column = value;
}

uint32_t tty_get_column(void)
{
    return column;
}

uint32_t tty_row_amount(void)
{
    return frontbuffer.height / (TTY_CHAR_HEIGHT * scale);
}

uint32_t tty_column_amount(void)
{
    return frontbuffer.width / (TTY_CHAR_WIDTH * scale);
}

void tty_acquire(void)
{
    lock_acquire(&lock);
}

void tty_release(void)
{
    lock_release(&lock);
}

void tty_put(uint8_t chr)
{
    switch (chr)
    {
    case '\n':
    {
        column = 0;
        row++;
    }
    break;
    case '\r':
    {
        column = 0;
    }
    break;
    default:
    {               
        char const* glyph = font.glyphs + chr * TTY_CHAR_HEIGHT;
        
        uint32_t x = column * TTY_CHAR_WIDTH * scale;
        uint32_t y = row * TTY_CHAR_HEIGHT * scale;

        for (uint32_t yOffset = 0; yOffset < TTY_CHAR_HEIGHT * scale; yOffset++)
        {
            for (uint32_t xOffset = 0; xOffset < TTY_CHAR_WIDTH * scale; xOffset++)
            {                
                Pixel pixel;
                if ((*glyph & (0b10000000 >> xOffset / scale)) > 0)
                {
                    pixel = foreground;
                }
                else
                {
                    pixel = background;
                }    
                
                *((Pixel*)((uint64_t)frontbuffer.base + 
                (x + xOffset) * sizeof(Pixel) + 
                (y + yOffset) * frontbuffer.pixelsPerScanline * sizeof(Pixel))) = pixel;
            }
            if (yOffset % scale == 0)
            {
                glyph++;
            }
        }

        column++;
    }
    break;
    }
}

void tty_print(const char* string)
{
    char* chr = (char*)string;
    while (*chr != '\0')
    {
        tty_put(*chr);
        chr++;
    }
}

void tty_printi(uint64_t integer)
{
    char string[64];
    itoa(integer, string, 10);
    tty_print(string);
}

void tty_printx(uint64_t hex)
{
    char string[64];
    memset(string, 0, 64);
    itoa(hex, string, 16);
    tty_print("0x"); tty_print(string);
}

void tty_printm(const char* string, uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        tty_put(string[i]);
    }
}

void tty_clear(void)
{
    memset(frontbuffer.base, 0, frontbuffer.size);
    column = 0;
    row = 0;
}

void tty_start_message(const char* message)
{
    tty_print("[..] ");
    tty_print(message);
    tty_print("... ");
}

void tty_assert(uint8_t expression, const char* message)
{
    if (!expression)
    {
        tty_print(message);
        tty_end_message(TTY_MESSAGE_ER);
    }
}

void tty_end_message(uint64_t status)
{
    uint32_t oldColumn = tty_get_column();
    tty_set_column(1);

    switch (status)
    {
    case TTY_MESSAGE_OK:
    {
        foreground.a = 255;
        foreground.r = 0;
        foreground.g = 255;
        foreground.b = 0;
        tty_print("OK");
    }
    break;
    case TTY_MESSAGE_ER:
    {
        foreground.a = 255;
        foreground.r = 255;
        foreground.g = 0;
        foreground.b = 0;
        tty_print("ER");
        asm volatile("hlt");
    }
    break;
    }

    foreground.a = 255;
    foreground.r = 255;
    foreground.g = 255;
    foreground.b = 255;

    tty_set_column(oldColumn);
    tty_print("done!\n");
}