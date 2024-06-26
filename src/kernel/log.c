#include "log.h"

#include "io.h"
#include "time.h"
#include "font.h"
#include "lock.h"
#include "smp.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/gfx.h>
#include <sys/proc.h>

#include <common/version.h>

static char buffer[LOG_BUFFER_LENGTH];
static uint64_t writeIndex;
static lock_t lock;

static surface_t surface;
static point_t point;
static psf_t font;

static bool screenEnabled;
static bool timeEnabled;

static void log_clear_line(uint64_t y, uint64_t height)
{
    for (uint64_t i = 0; i < height; i++)
    {
        memset(&surface.buffer[(y + i) * surface.stride], 0, surface.width * sizeof(pixel_t));
    }
}

static void log_write_to_screen(const char* str)
{
    uint64_t strLen = strlen(str);

    for (uint64_t i = 0; i < strLen; i++)
    {
        if (str[i] == '\n' || point.x >= surface.width - PSF_WIDTH)
        {
            point.y += PSF_HEIGHT;
            point.x = point.x >= surface.width - PSF_WIDTH ? PSF_WIDTH * 4 : 0;

            if (point.y >= surface.height - PSF_HEIGHT)
            {
                point.y = 0;
                log_clear_line(point.y, PSF_HEIGHT);
            }

            if (point.y <= surface.height - PSF_HEIGHT * 2)
            {
                log_clear_line(point.y + PSF_HEIGHT, PSF_HEIGHT);
            }

            continue;
        }

        gfx_psf_char(&surface, &font, &point, str[i]);
        point.x += PSF_WIDTH;
    }
}

static void log_write_to_buffer(const char* str)
{
    LOCK_GUARD(&lock);

    const char* ptr = str;
    while (*ptr != '\0')
    {
#if CONFIG_LOG_SERIAL
        while ((io_inb(LOG_PORT + 5) & 0x20) == 0)
        {
        }
        io_outb(LOG_PORT, *ptr);
#endif

        buffer[writeIndex] = *ptr;
        writeIndex = (writeIndex + 1) % LOG_BUFFER_LENGTH;

        ptr++;
    }

    if (screenEnabled)
    {
        log_write_to_screen(str);
    }
}

static void log_print_va_padded_number(char** out, uint64_t padding, char chr, uint64_t number)
{
    char str[20];
    ulltoa(number, str, 10);

    uint64_t len = strlen(str);

    for (uint64_t i = 0; i < padding - len; i++)
    {
        *(*out)++ = chr;
    }
    for (uint64_t i = 0; i < len; i++)
    {
        *(*out)++ = str[i];
    }
}

static void log_print_va(const char* string, va_list args)
{
    char buffer[MAX_PATH];
    char* out = buffer;

    nsec_t time = timeEnabled ? time_uptime() : 0;
    nsec_t sec = time / SEC;
    nsec_t ms = (time % SEC) / (SEC / 1000);

    *out++ = '[';
    log_print_va_padded_number(&out, 11, ' ', sec);
    *out++ = '.';
    log_print_va_padded_number(&out, 4, '0', ms);
    *out++ = ']';
    *out++ = ' ';

    const char* ptr = string;
    while (*ptr != '\0')
    {
        if (*ptr != LOG_BREAK)
        {
            *out++ = *ptr++;
            continue;
        }

        ptr++;
        switch (*ptr)
        {
        case LOG_ADDR:
        {
            *out++ = '0';
            *out++ = 'x';

            uintptr_t addr = va_arg(args, uintptr_t);
            char addrStr[17];
            ulltoa(addr, addrStr, 16);

            uint64_t addrStrlen = strlen(addrStr);
            for (uint64_t i = 0; i < 16 - addrStrlen; i++)
            {
                *out++ = '0';
            }

            for (uint64_t i = 0; i < addrStrlen; i++)
            {
                *out++ = addrStr[i];
            }
        }
        break;
        case LOG_STR:
        {
            const char* str = va_arg(args, const char*);
            for (uint64_t i = 0; i < strlen(str); i++)
            {
                *out++ = str[i];
            }
        }
        break;
        case LOG_INT:
        {
            uint64_t num = va_arg(args, uint64_t);
            char numStr[17];
            ulltoa(num, numStr, 10);

            for (uint64_t i = 0; i < strlen(numStr); i++)
            {
                *out++ = numStr[i];
            }
        }
        break;
        default:
        {
            return;
        }
        }
        ptr++;
    }

    *out++ = '\n';
    *out++ = '\0';
    log_write_to_buffer(buffer);
}

#if CONFIG_LOG_SERIAL
static void log_serial_init()
{
    io_outb(LOG_PORT + 1, 0x00);
    io_outb(LOG_PORT + 3, 0x80);
    io_outb(LOG_PORT + 0, 0x03);
    io_outb(LOG_PORT + 1, 0x00);
    io_outb(LOG_PORT + 3, 0x03);
    io_outb(LOG_PORT + 2, 0xC7);
    io_outb(LOG_PORT + 4, 0x0B);
}
#endif

void log_init(void)
{
    writeIndex = 0;
    screenEnabled = false;
    timeEnabled = false;
    lock_init(&lock);

#if CONFIG_LOG_SERIAL
    log_serial_init();
#endif

    log_print(OS_NAME " - " OS_VERSION "");
    log_print("Licensed under GPLv3. See www.gnu.org/licenses/gpl-3.0.html.");
}

void log_enable_screen(gop_buffer_t* gopBuffer)
{
    if (gopBuffer != NULL)
    {
        surface.buffer = gopBuffer->base;
        surface.width = gopBuffer->width;
        surface.height = gopBuffer->height;
        surface.stride = gopBuffer->stride;
        memset(gopBuffer->base, 0, gopBuffer->size);

        point.x = 0;
        point.y = 0;

        font.foreground = 0xFFA3A4A3;
        font.background = 0xFF000000;
        font.scale = 1;
        font.glyphs = font_get() + sizeof(psf_header_t);
    }

    log_write_to_screen(buffer);

    screenEnabled = true;
}

void log_disable_screen(void)
{
    screenEnabled = false;
}

void log_enable_time(void)
{
    timeEnabled = true;
}

void log_panic(const char* string, ...)
{
    asm volatile("cli");
    if (smp_initialized())
    {
        smp_send_ipi_to_others(IPI_HALT);
    }

    if (surface.buffer != NULL && !screenEnabled)
    {
        log_enable_screen(NULL);
    }

    log_print("!!! KERNEL PANIC !!!");

    va_list args;
    va_start(args, string);
    log_print_va(string, args);
    va_end(args);

    while (1)
    {
        asm volatile("hlt");
    }
}

void log_print(const char* string, ...)
{
    va_list args;
    va_start(args, string);
    log_print_va(string, args);
    va_end(args);
}
