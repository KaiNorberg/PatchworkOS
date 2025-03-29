#include "log.h"

#include "com.h"
#include "defs.h"
#include "font.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "systime.h"

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

    // System context
    printf("[SYSTEM STATE]");
    if (smp_initialized())
    {
        thread_t* thread = sched_thread();
        if (thread != NULL)
        {
            printf("thread: cpu=%d pid=%d tid=%d", smp_self_unsafe()->id, thread->process->id, thread->id);
        }
        else
        {
            printf("thread: CPU=%d IDLE", smp_self_unsafe()->id);
        }
    }
    else
    {
        printf("thread: occured before smp_init, assume CPU 0", smp_self_unsafe()->id);
    }

    printf("memory: free=%dKB reserved=%dKB", (pmm_free_amount() * PAGE_SIZE) / 1024, (pmm_reserved_amount() * PAGE_SIZE) / 1024);
    printf("control regs: cr0=0x%016lx cr2=0x%016lx cr3=0x%016lx cr4=0x%016lx", cr0_read(), cr2_read(), cr3_read(),
        cr4_read());

    if (trapFrame)
    {
        printf("[TRAP FRAME]");
        printf("vector=0x%02lx error=0x%016lx", trapFrame->vector, trapFrame->errorCode);
        printf("rflags=0x%016lx", trapFrame->rflags);
        printf("rip=0x%016lx cs =%04lx", trapFrame->rip, trapFrame->cs);
        printf("rsp=0x%016lx ss =%04lx", trapFrame->rsp, trapFrame->ss);
        printf("rax=0x%016lx rbx=0x%016lx rcx=0x%016lx rdx=0x%016lx", trapFrame->rax, trapFrame->rbx, trapFrame->rcx,
            trapFrame->rdx);
        printf("rsi=0x%016lx rdi=0x%016lx rbp=0x%016lx", trapFrame->rsi, trapFrame->rdi, trapFrame->rbp);
        printf("r8 =0x%016lx r9 =0x%016lx r10=0x%016lx r11=0x%016lx", trapFrame->r8, trapFrame->r9, trapFrame->r10, trapFrame->r11);
        printf("r12=0x%016lx r13=0x%016lx r14=0x%016lx r15=0x%016lx", trapFrame->r12, trapFrame->r13, trapFrame->r14,
            trapFrame->r15);
    }

    printf("[STACK TRACE]");
    uint64_t* frame = (uint64_t*)__builtin_frame_address(0);
    for (uint64_t i = 0; i < 16; i++)
    {
        if (frame == NULL || frame[1] == 0)
        {
            break;
        }
        printf("#%02d %016lx", i, frame[1]);
        frame = (uint64_t*)frame[0];
    }

    printf("!!! KERNEL PANIC END - Please restart your machine !!!");
    while (1)
    {
        asm volatile("hlt");
    }
}
