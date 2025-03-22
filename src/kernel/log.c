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
#include <stdio.h>
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
static atomic_bool panicking;

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

void log_init(void)
{
    writeIndex = 0;
    screenEnabled = false;
    timeEnabled = false;
    lock_init(&lock);
    atomic_init(&panicking, false);

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    printf(OS_NAME " - " OS_VERSION "");
    printf("Licensed under GPLv3. See www.gnu.org/licenses/gpl-3.0.html.");
}

void log_enable_screen(gop_buffer_t* gopBuffer)
{
    LOCK_DEFER(&lock);

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
    LOCK_DEFER(&lock);
    screenEnabled = false;
}

void log_enable_time(void)
{
    LOCK_DEFER(&lock);
    timeEnabled = true;
}

bool log_time_enabled(void)
{
    return timeEnabled;
}

void log_write(const char* str)
{
    LOCK_DEFER(&lock);

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

NORETURN void log_panic(const trap_frame_t* trapFrame, const char* string, ...)
{
    asm volatile("cli");

    if (atomic_exchange(&panicking, true))
    {
        while (true)
        {
            asm volatile("hlt");
        }
    }

    if (smp_initialized())
    {
        smp_halt_others();
    }

    if (gfx.buffer != NULL && !screenEnabled)
    {
        log_enable_screen(NULL);
    }

    char bigString[MAX_PATH];
    strcpy(bigString, "!!! KERNEL PANIC - ");
    strcat(bigString, string);
    strcat(bigString, " !!!");

    va_list args;
    va_start(args, string);
    vprintf(bigString, args);
    va_end(args);

    if (smp_initialized())
    {
        thread_t* thread = sched_thread();
        if (thread == NULL)
        {
            printf("Occured on cpu %d while idle", smp_self_unsafe()->id);
        }
        else
        {
            printf("Occured on cpu %d in process %d thread %d", smp_self_unsafe()->id, thread->process->id, thread->id);
        }
    }
    else
    {
        printf("Occured before smp init, assumed cpu 0");
    }

    printf("pmm: free %d, reserved %d", pmm_free_amount(), pmm_reserved_amount());

    if (trapFrame != NULL)
    {
        printf("ss %p, rsp %p, rflags %p, cs %p, rip %p", trapFrame->ss, trapFrame->rsp, trapFrame->rflags, trapFrame->cs,
            trapFrame->rip);
        printf("error code %p, vector %p", trapFrame->errorCode, trapFrame->vector);
        printf("rax %p, rbx %p, rcx %p, rdx %p, rsi %p, rdi %p, rbp %p", trapFrame->rax, trapFrame->rbx, trapFrame->rcx,
            trapFrame->rdx, trapFrame->rsi, trapFrame->rdi, trapFrame->rbp);
        printf("r8 %p, r9 %p, r10 %p, r11 %p, r12 %p, r13 %p, r14 %p, r15 %p", trapFrame->r8, trapFrame->r9, trapFrame->r10,
            trapFrame->r11, trapFrame->r12, trapFrame->r13, trapFrame->r14, trapFrame->r15);
    }

    printf("cr0: %p, cr2: %p, cr3: %p, cr4: %p", cr0_read(), cr2_read(), cr3_read(), cr4_read());

    printf("call stack:");
    uint64_t* frame = (uint64_t*)__builtin_frame_address(0);
    for (uint64_t i = 0; i < 16; i++)
    {
        if (frame == NULL)
        {
            break;
        }

        printf("%p", frame[1]);
        frame = (uint64_t*)frame[0];
    }

    printf("Please restart your machine");

    while (1)
    {
        asm volatile("hlt");
    }
}
