#include "terminal.h"

#include "fb.h"

#include <string.h>
#include <stdlib.h>
#include <sys/io.h>

#include <libs/std/internal/syscalls.h>

static uint64_t column;
static uint64_t row;

void terminal_init()
{
    column = 0;
    row = 0;
}

void terminal_put(const char chr)
{
    switch (chr)
    {
    case '\n':
    {
        column = 0;
        row++;

        if (row >= fb_height() / FB_CHAR_HEIGHT)
        {
            row -= 2;
            fb_scroll(FB_CHAR_HEIGHT * 4);
        }
    }
    break;
    case '\r':
    {
        column = 0;
    }
    break;
    default:
    {               
        fb_char(chr, column * FB_CHAR_WIDTH, row * FB_CHAR_HEIGHT, UINT32_MAX, 0);
        column++;
        
        if (column >= fb_width() / FB_CHAR_WIDTH)
        {
            terminal_put('\n');
        }
    }
    break;
    }
}

void terminal_print(const char* string)
{    
    for (uint64_t i = 0; i < strlen(string); i++)
    {
        terminal_put(string[i]);
    }
}

__attribute__((noreturn)) void terminal_loop()
{
    terminal_print("Hello from the user space shell!\n");

    fd_t keyboard = open("A:/keyboard/ps2");

    while (1)
    {
        uint8_t key;
        if (read(keyboard, &key, sizeof(uint8_t)) == sizeof(uint8_t))
        {
            char string[32];
            lltoa(key, string, 10);
            terminal_print(string);
            terminal_put('\n');
        }
    }
}