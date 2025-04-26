#include "log.h"

#include "bootloader/boot_info.h"
#include "com.h"
#include "defs.h"
#include "font.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "systime.h"

#include <ring.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

#include <common/version.h>

static char ringBuffer[LOG_BUFFER_LENGTH];
static ring_t ring;

static gop_buffer_t gop;
static uint64_t posX;
static uint64_t posY;

static bool screenEnabled;
static bool timeEnabled;
static atomic_bool panicking;

static lock_t lock;

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

static void log_draw_char(char chr);

static void log_clear_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    width = MIN(width, (gop.width - x));
    for (uint64_t i = 0; i < height; i++)
    {
        memset(&gop.base[x + (y + i) * gop.stride], 0, (width) * sizeof(pixel_t));
    }
}

// Also handles scrolling
static void log_redraw(void)
{
    posY = 0;
    posX = 0;

    int64_t lineAmount = 0;
    for (uint64_t i = 0; i < ring.dataLength; i++)
    {
        uint8_t byte = ((uint8_t*)ring.buffer)[(ring.readIndex + i) % ring.size];
        if (byte == '\n' || posX >= gop.width - FONT_WIDTH)
        {
            lineAmount++;
            posY += FONT_HEIGHT;
            posX = posX >= gop.width - FONT_WIDTH ? FONT_WIDTH * 4 : 0; // Add indentation if wrapping to next line
        }

        if (byte != '\n')
        {
            posX += FONT_WIDTH;
        }
    }

    posY = 0;
    posX = 0;

    uint64_t amountOfLinesToSkip = MAX(0, lineAmount - ((int64_t)gop.height / FONT_HEIGHT - LOG_SCROLL_OFFSET));
    uint64_t i = 0;
    if (amountOfLinesToSkip != 0)
    {
        for (; i < ring.dataLength; i++)
        {
            uint8_t byte = ((uint8_t*)ring.buffer)[(ring.readIndex + i) % ring.size];
            if (byte != '\n')
            {
                continue;
            }

            amountOfLinesToSkip--;
            if (amountOfLinesToSkip == 0)
            {
                i++;
                break;
            }
        }
    }
    else
    {
        for (; i < ring.dataLength; i++)
        {
            uint8_t byte = ((uint8_t*)ring.buffer)[(ring.readIndex + i) % ring.size];
            if (byte == '\n')
            {
                i++;
                break;
            }
        }
    }

    uint64_t lineWidth = 0;
    for (; i < ring.dataLength; i++)
    {
        uint8_t byte = ((uint8_t*)ring.buffer)[(ring.readIndex + i) % ring.size];
        if (byte == '\n')
        {
            if (lineWidth < LOG_MAX_LINE)
            {
                uint64_t width = MIN(LOG_MAX_LINE * FONT_WIDTH, gop.width) - posX;
                log_clear_rect(posX, posY, width, FONT_HEIGHT);
            }
            lineWidth = 0;
        }
        else
        {
            lineWidth++;
        }

        log_draw_char(byte);
    }

    for (uint64_t y = posY; y < gop.height - FONT_HEIGHT; y += FONT_HEIGHT)
    {
        log_clear_rect(posX, y, LOG_MAX_LINE * FONT_WIDTH, FONT_HEIGHT);
    }
}

static void log_draw_char(char chr)
{
    if (chr == '\n' || posX >= gop.width - FONT_WIDTH)
    {
        posY += FONT_HEIGHT;
        posX = posX >= gop.width - FONT_WIDTH ? FONT_WIDTH * 4 : 0; // Add indentation if wrapping to next line

        if (posY >= gop.height - FONT_HEIGHT)
        {
            log_redraw();
        }
    }

    if (chr != '\n')
    {
        const uint8_t* glyph = font_glyphs() + chr * FONT_HEIGHT;

        for (uint64_t y = 0; y < FONT_HEIGHT; y++)
        {
            for (uint64_t x = 0; x < FONT_WIDTH; x++)
            {
                pixel_t pixel = (glyph[y] & (0b10000000 >> x)) > 0 ? 0xFFA3A4A3 : 0;
                gop.base[(posX + x) + (posY + y) * gop.stride] = pixel;
            }
        }
        posX += FONT_WIDTH;
    }
}

static void log_draw_string(const char* str)
{
    while (*str != '\0')
    {
        log_draw_char(*str);
        str++;
    }
}

void log_init(void)
{
    ring_init(&ring, ringBuffer, LOG_BUFFER_LENGTH);
    screenEnabled = false;
    timeEnabled = false;
    lock_init(&lock);
    atomic_init(&panicking, false);
    gop.base = NULL;

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    printf(OS_NAME " - " OS_VERSION "");
    printf("Licensed under MIT. See home:/usr/license/LICENSE.");
}

static uint64_t log_read(file_t* file, void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);

    uint64_t result = ring_read_at(&ring, file->pos, buffer, count);
    file->pos += result;
    return result;
}

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(klogOps, (file_ops_t){
    .read = log_read,
});

void log_expose(void)
{
    sysobj_new("/", "klog", &klogOps, NULL);
}

void log_enable_screen(gop_buffer_t* gopBuffer)
{
    printf("log: enable screen");
    LOCK_DEFER(&lock);

    if (gopBuffer != NULL)
    {
        gop = *gopBuffer;
    }
    memset(gop.base, 0, gop.height * gop.height * sizeof(pixel_t));

    posX = 0;
    posY = 0;

    log_redraw();
    screenEnabled = true;
}

void log_disable_screen(void)
{
    printf("log: disable screen");
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

void log_print(const char* str)
{
    ASSERT_PANIC(strlen(str) < LOG_MAX_LINE);

    LOCK_DEFER(&lock);

    const char* ptr = str;
    while (*ptr != '\0')
    {
#if CONFIG_LOG_SERIAL
        com_write(COM1, *ptr);
#endif

        ring_write(&ring, ptr, 1);
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

    smp_halt_others();
    if (gop.base != NULL && !screenEnabled)
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

    // System ctx
    printf("[SYSTEM STATE]");
    thread_t* thread = sched_thread();
    if (thread != NULL)
    {
        printf("thread: cpu=%d pid=%d tid=%d", smp_self_unsafe()->id, thread->process->id, thread->id);
    }
    else
    {
        printf("thread: CPU=%d IDLE", smp_self_unsafe()->id);
    }

    printf("memory: free=%dKB reserved=%dKB", (pmm_free_amount() * PAGE_SIZE) / 1024, (pmm_reserved_amount() * PAGE_SIZE) / 1024);
    printf("control regs: cr0=0x%016lx cr2=0x%016lx cr3=0x%016lx cr4=0x%016lx", cr0_read(), cr2_read(), cr3_read(), cr4_read());

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
        printf("r8 =0x%016lx r9 =0x%016lx r10=0x%016lx r11=0x%016lx", trapFrame->r8, trapFrame->r9, trapFrame->r10,
            trapFrame->r11);
        printf("r12=0x%016lx r13=0x%016lx r14=0x%016lx r15=0x%016lx", trapFrame->r12, trapFrame->r13, trapFrame->r14,
            trapFrame->r15);
    }

    printf("[STACK TRACE]");
    void* frame = __builtin_frame_address(0);
    uint64_t frameNum = 0;
    while (frame != NULL && frameNum < 64)
    {
        if ((uintptr_t)frame & 0x7)
        {
            printf("[MISALIGNED FRAME: 0x%016lx]", (uintptr_t)frame);
            break;
        }

        void* returnAddr = *((void**)frame + 1);
        if (returnAddr != NULL && (returnAddr >= (void*)&_kernelStart && returnAddr < (void*)&_kernelEnd))
        {
            printf("#%02d: [0x%016lx]", frameNum, returnAddr);
        }
        else
        {
            printf("[STACK TRACE END: 0x%016lx]", returnAddr);
            break;
        }

        frame = *((void**)frame);
        frameNum++;
    }

    printf("!!! KERNEL PANIC END - Please restart your machine !!!");
    while (1)
    {
        asm volatile("hlt");
    }
}
