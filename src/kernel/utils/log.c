#include "log.h"

#include "bootloader/boot_info.h"
#include "cpu/port.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "defs.h"
#include "drivers/com.h"
#include "drivers/systime/systime.h"
#include "font.h"
#include "mem/pmm.h"
#include "sched/sched.h"
#include "sync/lock.h"
#include "utils/font.h"
#include "utils/ring.h"

#include <common/version.h>

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>

static char ringBuffer[LOG_BUFFER_LENGTH];
static ring_t ring;

static gop_buffer_t gop;
static uint64_t posX;
static uint64_t posY;

static bool isScreenEnabled;
static bool isTimeEnabled;
static bool isLastCharWasNewline;

static atomic_bool panicking;

static lock_t lock;

static sysobj_t klog;

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

static void log_draw_char(char chr);

static void log_clear_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    width = MIN(width, (gop.width - x));
    for (uint64_t i = 0; i < height; i++)
    {
        memset(&gop.base[x + (y + i) * gop.stride], 0, (width) * sizeof(uint32_t));
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
        if (chr < 0)
        {
            chr = ' ';
        }

        const uint8_t* glyph = font_glyphs() + chr * FONT_HEIGHT;

        for (uint64_t y = 0; y < FONT_HEIGHT; y++)
        {
            for (uint64_t x = 0; x < FONT_WIDTH; x += 2)
            {
                // Draw two pixels at a time.
                uint64_t pixel1 = (glyph[y] & (0b10000000 >> x)) > 0 ? LOG_TEXT_COLOR : 0;
                uint64_t pixel2 = (glyph[y] & (0b10000000 >> (x + 1))) > 0 ? LOG_TEXT_COLOR : 0;
                *((uint64_t*)&gop.base[(posX + x) + (posY + y) * gop.stride]) = (pixel2 << 32 | pixel1);
            }
        }
        posX += FONT_WIDTH;
    }
}

void log_init(void)
{
    ring_init(&ring, ringBuffer, LOG_BUFFER_LENGTH);
    isScreenEnabled = false;
    isTimeEnabled = false;
    isLastCharWasNewline = false;
    lock_init(&lock);
    atomic_init(&panicking, false);
    gop.base = NULL;

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    printf(OS_NAME " - " OS_VERSION "\n");
    printf("Licensed under MIT. See home:/usr/license/LICENSE.\n");
}

static uint64_t log_read(file_t* file, void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);

    uint64_t result = ring_read_at(&ring, file->pos, buffer, count);
    file->pos += result;
    return result;
}

static uint64_t log_write(file_t* file, const void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }
    if (count >= LOG_MAX_LINE)
    {
        return ERROR(EINVAL);
    }
    char string[LOG_MAX_LINE];
    for (uint64_t i = 0; i < count; i++)
    {
        char chr = ((char*)buffer)[i];
        string[i] = chr;
    }
    string[count] = '\0';
    printf(string);
    return count;
}

SYSFS_STANDARD_OPS_DEFINE(klogOps, PATH_NONE,
    (file_ops_t){
        .read = log_read,
        .write = log_write,
    });

void log_expose(void)
{
    printf("log: expose\n");
    assert(sysobj_init_path(&klog, "/", "klog", &klogOps, NULL) != ERR);
}

void log_enable_screen(gop_buffer_t* gopBuffer)
{
    printf("log: enable screen\n");
    LOCK_DEFER(&lock);

    if (gopBuffer != NULL)
    {
        gop = *gopBuffer;
    }
    memset(gop.base, 0, gop.stride * gop.height * sizeof(uint32_t));

    posX = 0;
    posY = 0;

    log_redraw();
    isScreenEnabled = true;
}

void log_disable_screen(void)
{
    if (isScreenEnabled)
    {
        printf("log: disable screen\n");
        isScreenEnabled = false;
    }
}

void log_enable_time(void)
{
    LOCK_DEFER(&lock);
    isTimeEnabled = true;
}

bool log_is_time_enabled(void)
{
    return isTimeEnabled;
}

static void log_put(char ch)
{
    if (isLastCharWasNewline)
    {
        isLastCharWasNewline = false;
        char buffer[MAX_PATH];
        clock_t time = log_is_time_enabled() ? systime_uptime() : 0;
        clock_t sec = time / CLOCKS_PER_SEC;
        clock_t ms = (time % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);
        sprintf(buffer, "[%10llu.%03llu] ", sec, ms);

        char* ptr = buffer;
        while (*ptr != '\0')
        {
            log_put(*ptr);
            ptr++;
        }
    }

#if CONFIG_LOG_SERIAL
    com_write(COM1, ch);
#endif

    ring_write(&ring, &ch, 1);

    if (ch == '\n')
    {
        isLastCharWasNewline = true;
    }

    if (isScreenEnabled)
    {
        log_draw_char(ch);
    }
}

void log_print(const char* str)
{
    assert(strlen(str) < LOG_MAX_LINE);

    LOCK_DEFER(&lock);

    const char* ptr = str;
    while (*ptr != '\0')
    {
        log_put(*ptr);
        ptr++;
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
    if (gop.base != NULL && !isScreenEnabled)
    {
        log_enable_screen(NULL);
    }

    char bigString[MAX_PATH];
    strcpy(bigString, "!!! KERNEL PANIC - ");
    strcat(bigString, string);
    strcat(bigString, " !!!\n");
    va_list args;
    va_start(args, string);
    vprintf(bigString, args);
    va_end(args);

    // System ctx
    printf("[SYSTEM STATE]\n");
    thread_t* thread = sched_thread();
    if (thread != NULL)
    {
        printf("thread: cpu=%d pid=%d tid=%d\n", smp_self_unsafe()->id, thread->process->id, thread->id);
    }
    else
    {
        printf("thread: CPU=%d IDLE\n", smp_self_unsafe()->id);
    }

    printf("memory: free=%dKB reserved=%dKB\n", (pmm_free_amount() * PAGE_SIZE) / 1024,
        (pmm_reserved_amount() * PAGE_SIZE) / 1024);
    printf("control regs: cr0=0x%016lx cr2=0x%016lx cr3=0x%016lx cr4=0x%016lx\n", cr0_read(), cr2_read(), cr3_read(),
        cr4_read());

    if (trapFrame)
    {
        printf("[TRAP FRAME]\n");
        printf("vector=0x%02lx error=0x%016lx\n", trapFrame->vector, trapFrame->errorCode);
        printf("rflags=0x%016lx\n", trapFrame->rflags);
        printf("rip=0x%016lx cs =%04lx\n", trapFrame->rip, trapFrame->cs);
        printf("rsp=0x%016lx ss =%04lx\n", trapFrame->rsp, trapFrame->ss);
        printf("rax=0x%016lx rbx=0x%016lx rcx=0x%016lx rdx=0x%016lx\n", trapFrame->rax, trapFrame->rbx, trapFrame->rcx,
            trapFrame->rdx);
        printf("rsi=0x%016lx rdi=0x%016lx rbp=0x%016lx\n", trapFrame->rsi, trapFrame->rdi, trapFrame->rbp);
        printf("r8 =0x%016lx r9 =0x%016lx r10=0x%016lx r11=0x%016lx\n", trapFrame->r8, trapFrame->r9, trapFrame->r10,
            trapFrame->r11);
        printf("r12=0x%016lx r13=0x%016lx r14=0x%016lx r15=0x%016lx\n", trapFrame->r12, trapFrame->r13, trapFrame->r14,
            trapFrame->r15);
    }

    printf("[STACK TRACE]\n");
    void* frame = __builtin_frame_address(0);
    uint64_t frameNum = 0;
    while (frame != NULL && frameNum < 64)
    {
        if ((uintptr_t)frame & 0x7)
        {
            printf("[MISALIGNED FRAME: 0x%016lx]\n", (uintptr_t)frame);
            break;
        }

        void* returnAddr = *((void**)frame + 1);
        if (returnAddr != NULL && (returnAddr >= (void*)&_kernelStart && returnAddr < (void*)&_kernelEnd))
        {
            printf("#%02d: [0x%016lx]\n", frameNum, returnAddr);
        }
        else
        {
            printf("[STACK TRACE END: 0x%016lx]\n", returnAddr);
            break;
        }

        frame = *((void**)frame);
        frameNum++;
    }

    printf("!!! KERNEL PANIC END - Please restart your machine !!!\n");
#ifdef QEMU_ISA_DEBUG_EXIT
    port_outb(QEMU_ISA_DEBUG_EXIT_PORT, EXIT_FAILURE);
#endif
    while (1)
    {
        asm volatile("hlt");
    }
}
