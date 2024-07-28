#include "log.h"

#include "com.h"
#include "defs.h"
#include "font.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "time.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/proc.h>

#include <common/version.h>

static char buffer[LOG_BUFFER_LENGTH];
static uint64_t writeIndex;

static gfx_t gfx;
static point_t point;

static bool screenEnabled;
static bool timeEnabled;

static lock_t lock;

static void log_clear_line(uint64_t y, uint64_t height)
{
    for (uint64_t i = 0; i < height; i++)
    {
        memset(&gfx.buffer[(y + i) * gfx.stride], 0, gfx.width * sizeof(pixel_t));
    }
}

static void log_draw_char(char chr)
{
    const uint8_t* glyph = font_glyphs() + chr * FONT_HEIGHT;

    for (uint64_t y = 0; y < FONT_HEIGHT; y++)
    {
        for (uint64_t x = 0; x < FONT_WIDTH; x++)
        {
            pixel_t pixel = (glyph[y] & (0b10000000 >> x)) > 0 ? 0xFFA3A4A3 : 0;
            gfx.buffer[(point.x + x) + (point.y + y) * gfx.stride] = pixel;
        }
    }
    point.x += FONT_WIDTH;
}

static void log_draw_string(const char* str)
{
    while (*str != '\0')
    {
        if (*str == '\n' || point.x >= gfx.width - FONT_WIDTH)
        {
            point.y += FONT_HEIGHT;
            point.x = point.x >= gfx.width - FONT_WIDTH ? FONT_WIDTH * 4 : 0;

            if (point.y >= gfx.height - FONT_HEIGHT)
            {
                point.y = 0;
                log_clear_line(point.y, FONT_HEIGHT);
            }

            if (point.y <= gfx.height - FONT_HEIGHT * 2)
            {
                log_clear_line(point.y + FONT_HEIGHT, FONT_HEIGHT);
            }
        }

        if (*str != '\n')
        {
            log_draw_char(*str);
        }

        str++;
    }
}

static void log_write(const char* str)
{
    LOCK_GUARD(&lock);

    const char* ptr = str;
    while (*ptr != '\0')
    {
#if CONFIG_LOG_SERIAL
        com_write(COM1, *ptr);
#endif

        buffer[writeIndex] = *ptr;
        writeIndex = (writeIndex + 1) % LOG_BUFFER_LENGTH;

        ptr++;
    }

    if (screenEnabled)
    {
        log_draw_string(str);
    }
}

static void log_padd_number(char** out, uint64_t padding, char chr, uint64_t number)
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
    log_padd_number(&out, 11, ' ', sec);
    *out++ = '.';
    log_padd_number(&out, 3, '0', ms);
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
    log_write(buffer);
}

void log_init(void)
{
    writeIndex = 0;
    screenEnabled = false;
    timeEnabled = false;
    lock_init(&lock);

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    log_print(OS_NAME " - " OS_VERSION "");
    log_print("Licensed under GPLv3. See www.gnu.org/licenses/gpl-3.0.html.");
}

void log_enable_screen(gop_buffer_t* gopBuffer)
{
    LOCK_GUARD(&lock);

    if (gopBuffer != NULL)
    {
        gfx.buffer = gopBuffer->base;
        gfx.width = gopBuffer->width;
        gfx.height = gopBuffer->height;
        gfx.stride = gopBuffer->stride;
    }

    point.x = 0;
    point.y = 0;
    memset(gfx.buffer, 0, gfx.stride * gfx.height * sizeof(pixel_t));

    log_draw_string(buffer);

    screenEnabled = true;
}

void log_disable_screen(void)
{
    LOCK_GUARD(&lock);
    screenEnabled = false;
}

void log_enable_time(void)
{
    LOCK_GUARD(&lock);
    timeEnabled = true;
}

void log_print(const char* string, ...)
{
    va_list args;
    va_start(args, string);
    log_print_va(string, args);
    va_end(args);
}

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* string, ...)
{
    asm volatile("cli");
    if (smp_initialized())
    {
        smp_halt_others();
    }

    if (gfx.buffer != NULL && !screenEnabled)
    {
        log_enable_screen(NULL);
    }

    char buffer[MAX_PATH];
    strcpy(buffer, "!!! KERNEL PANIC - ");
    strcat(buffer, string);
    strcat(buffer, " !!!");
    va_list args;
    va_start(args, string);
    log_print_va(buffer, args);
    va_end(args);

    if (smp_initialized())
    {
        thread_t* thread = sched_thread();
        if (thread == NULL)
        {
            log_print("Occured on cpu %d while idle", smp_self_unsafe()->id);
        }
        else
        {
            log_print("Occured on cpu %d in process %d", smp_self_unsafe()->id, thread->process->id);
        }
    }
    else
    {
        log_print("Occured before smp init, assumed cpu 0");
    }

    log_print("pmm: free %d, reserved %d", pmm_free_amount(), pmm_reserved_amount());

    if (trapFrame != NULL)
    {
        log_print("ss %a, rsp %a, rflags %a, cs %a, rip %a", trapFrame->ss, trapFrame->rsp, trapFrame->rflags, trapFrame->cs,
            trapFrame->rip);
        log_print("error code %a, vector %a", trapFrame->errorCode, trapFrame->vector);
        log_print("rax %a, rbx %a, rcx %a, rdx %a, rsi %a, rdi %a, rbp %a", trapFrame->rax, trapFrame->rbx, trapFrame->rcx,
            trapFrame->rdx, trapFrame->rsi, trapFrame->rdi, trapFrame->rbp);
        log_print("r8 %a, r9 %a, r10 %a, r11 %a, r12 %a, r13 %a, r14 %a, r15 %a", trapFrame->r8, trapFrame->r9, trapFrame->r10,
            trapFrame->r11, trapFrame->r12, trapFrame->r13, trapFrame->r14, trapFrame->r15);
    }

    log_print("cr0: %a, cr2: %a, cr3: %a, cr4: %a", cr0_read(), cr2_read(), cr3_read(), cr4_read());

    log_print("Call Stack:");
    uint64_t* frame = (uint64_t*)__builtin_frame_address(0);
    for (uint64_t i = 0; i < 16; i++)
    {
        if (frame == NULL)
        {
            break;
        }

        log_print("%a", frame[1]);
        frame = (uint64_t*)frame[0];
    }

    log_print("Please restart your machine");

    while (1)
    {
        asm volatile("hlt");
    }
}
