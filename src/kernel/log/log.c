#include "log.h"

#include "cpu/port.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "defs.h"
#include "drivers/com.h"
#include "drivers/systime/systime.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <boot/boot_info.h>
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

static log_state_t state = {0};
static log_obj_t obj = {0};
static screen_t screen = {0};

static lock_t lock;

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

void log_init(void)
{
    lock_init(&lock);

    atomic_init(&state.panicking, false);
    state.config.isTimeEnabled = false;
    state.config.outputs = LOG_OUTPUT_SERIAL; // Serial will only do anything if CONFIG_LOG_SERIAL is true.
#ifdef DEBUG
    state.config.minLevel = LOG_DEBUG;
#else
    state.config.minLevel = LOG_USER;
#endif
    state.isLastCharNewline = true;

    ring_init(&obj.ring, obj.buffer, LOG_MAX_BUFFER);
    state.config.outputs |= LOG_OUTPUT_OBJ;

#if CONFIG_LOG_SERIAL
    com_init(COM1);
#endif

    log_print(LOG_INFO, "%s - %s\n", OS_NAME, OS_VERSION);
    log_print(LOG_INFO, "Licensed under MIT. See home:/usr/license/LICENSE.\n");
}

void log_enable_time(void)
{
    LOCK_DEFER(&lock);

    state.config.isTimeEnabled = true;
}

void log_screen_enable(gop_buffer_t* framebuffer)
{
    LOCK_DEFER(&lock);

    if (!screen.initialized)
    {
        assert(screen_init(&screen, framebuffer) != ERR);
    }

    screen_enable(&screen, &obj.ring);
    state.config.outputs |= LOG_OUTPUT_SCREEN;
}

void log_disable_screen(void)
{
    LOCK_DEFER(&lock);

    screen_disable(&screen);
    state.config.outputs &= ~LOG_OUTPUT_SCREEN;
}

static uint64_t log_obj_read(file_t* file, void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);

    uint64_t result = ring_read_at(&obj.ring, file->pos, buffer, count);
    file->pos += result;
    return result;
}

static uint64_t log_obj_write(file_t* file, const void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }
    if (count >= MAX_PATH)
    {
        return ERROR(EINVAL);
    }
    char string[MAX_PATH];
    memcpy(string, buffer, count);
    string[count] = '\0';
    log_print(LOG_USER, "%s", string);
    return count;
}

SYSFS_STANDARD_OPS_DEFINE(ops, PATH_NONE,
    (file_ops_t){
        .read = log_obj_read,
        .write = log_obj_write,
    });

void log_obj_expose(void)
{
    LOCK_DEFER(&lock);

    assert(sysobj_init_path(&obj.obj, "/", "klog", &ops, NULL) != ERR);
}

static void log_print_to_outputs(const char* string, uint64_t length)
{
    if (state.config.outputs & LOG_OUTPUT_OBJ)
    {
        ring_write(&obj.ring, string, length);
    }

#if CONFIG_LOG_SERIAL
    if (state.config.outputs & LOG_OUTPUT_SERIAL)
    {
        for (uint64_t i = 0; i < length; i++)
        {
            com_write(COM1, string[i]);
        }
    }
#endif

    if (state.config.outputs & LOG_OUTPUT_SCREEN)
    {
        screen_write(&screen, string, length);
    }
}

static void log_print_header(log_level_t level)
{
    if (level == LOG_PANIC)
    {
        return;
    }

    static const char* levelNames[] = {
        [LOG_DEBUG] = "DBUG",
        [LOG_USER] = "USER",
        [LOG_INFO] = "INFO",
        [LOG_WARN] = "WARN",
        [LOG_ERR] = "ERR ",
    };

    uint64_t uptime = state.config.isTimeEnabled ? systime_uptime() : 0;

    uint64_t seconds = uptime / CLOCKS_PER_SEC;
    uint64_t milliseconds = (uptime % CLOCKS_PER_SEC) / (CLOCKS_PER_SEC / 1000);

    cpu_t* self = smp_self_unsafe();

    int length =
        sprintf(state.timestampBuffer, "[%8llu.%03llu-%03d-%s] ", seconds, milliseconds, self->id, levelNames[level]);

    log_print_to_outputs(state.timestampBuffer, length);
}

static void log_handle_char(log_level_t level, char chr)
{
    if (state.isLastCharNewline && chr != '\n')
    {
        state.isLastCharNewline = false;

        log_print_header(level);
    }

    if (chr == '\n')
    {
        state.isLastCharNewline = true;
    }

    log_print_to_outputs(&chr, 1);
}

uint64_t log_print(log_level_t level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = log_vprint(level, format, args);
    va_end(args);
    return result;
}

uint64_t log_vprint(log_level_t level, const char* format, va_list args)
{
    LOCK_DEFER(&lock);

    if (level < state.config.minLevel)
    {
        return 0;
    }

    int length = vsnprintf(state.lineBuffer, LOG_MAX_BUFFER, format, args);
    if (length < 0)
    {
        return ERROR(EINVAL);
    }

    if (length >= LOG_MAX_BUFFER)
    {
        length = LOG_MAX_BUFFER - 1;
        state.lineBuffer[length] = '\0';
    }

    for (int i = 0; i < length; i++)
    {
        log_handle_char(level, state.lineBuffer[i]);
    }

    return 0;
}

static void log_panic_trap_frame(const trap_frame_t* trapFrame)
{
    log_print(LOG_PANIC, "[TRAP FRAME]\n");
    log_print(LOG_PANIC, "vector=0x%02llx error=0x%016llx\n", trapFrame->vector, trapFrame->errorCode);
    log_print(LOG_PANIC, "rflags=0x%016llx\n", trapFrame->rflags);
    log_print(LOG_PANIC, "rip=0x%016llx cs=%04llx\n", trapFrame->rip, trapFrame->cs);
    log_print(LOG_PANIC, "rsp=0x%016llx ss=%04llx\n", trapFrame->rsp, trapFrame->ss);
    log_print(LOG_PANIC, "rax=0x%016llx rbx=0x%016llx rcx=0x%016llx rdx=0x%016llx\n", trapFrame->rax, trapFrame->rbx,
        trapFrame->rcx, trapFrame->rdx);
    log_print(LOG_PANIC, "rsi=0x%016llx rdi=0x%016llx rbp=0x%016llx\n", trapFrame->rsi, trapFrame->rdi, trapFrame->rbp);
    log_print(LOG_PANIC, "r8=0x%016llx r9=0x%016llx r10=0x%016llx r11=0x%016llx\n", trapFrame->r8, trapFrame->r9,
        trapFrame->r10, trapFrame->r11);
    log_print(LOG_PANIC, "r12=0x%016llx r13=0x%016llx r14=0x%016llx r15=0x%016llx\n", trapFrame->r12, trapFrame->r13,
        trapFrame->r14, trapFrame->r15);
}

static void log_panic_trap_stack_trace(const trap_frame_t* trapFrame)
{
    log_print(LOG_PANIC, "[TRAP FRAME STACK TRACE]\n");
    log_print(LOG_PANIC, "RSP: 0x%016llx\n", trapFrame->rsp);

    uint64_t* rbp = (uint64_t*)trapFrame->rbp;
    uint64_t frameNumber = 0;

    while (rbp != NULL && frameNumber < LOG_MAX_STACK_FRAMES)
    {
        if ((uintptr_t)rbp < PAGE_SIZE || (uintptr_t)rbp >= PML_HIGHER_HALF_END ||
            ((uintptr_t)rbp > PML_LOWER_HALF_END && (uintptr_t)rbp < PML_LOWER_HALF_START))
        {
            log_print(LOG_PANIC, "[INVALID FRAME: 0x%016llx]\n", (uintptr_t)rbp);
            break;
        }

        if ((uintptr_t)rbp & 0x7)
        {
            log_print(LOG_PANIC, "[MISALIGNED FRAME: 0x%016llx]\n", (uintptr_t)rbp);
            break;
        }

        uint64_t returnAddress = rbp[1];
        uint64_t nextFramePointer = rbp[0];

        if (returnAddress != 0)
        {
            log_print(LOG_PANIC, "#%02llu: [0x%016llx]\n", frameNumber, returnAddress);
        }
        else
        {
            log_print(LOG_PANIC, "[TRAP FRAME STACK TRACE END]\n");
            break;
        }

        rbp = (uint64_t*)nextFramePointer;
        frameNumber++;
    }
}

static void log_panic_direct_stack_trace(void)
{
    log_print(LOG_PANIC, "[DIRECT STACK TRACE]\n");

    void* currentFrame = __builtin_frame_address(0);
    uint64_t frameNumber = 0;

    while (currentFrame != NULL && frameNumber < LOG_MAX_STACK_FRAMES)
    {
        if ((uintptr_t)currentFrame & 0x7)
        {
            log_print(LOG_PANIC, "[MISALIGNED FRAME: 0x%016llx]\n", (uintptr_t)currentFrame);
            break;
        }

        void* returnAddress = *((void**)currentFrame + 1);

        if (returnAddress != NULL && (returnAddress >= (void*)&_kernelStart && returnAddress < (void*)&_kernelEnd))
        {
            log_print(LOG_PANIC, "#%02llu: [0x%016llx]\n", frameNumber, (uintptr_t)returnAddress);
        }
        else
        {

            log_print(LOG_PANIC, "[DIRECT STACK TRACE END: 0x%016llx]\n", (uintptr_t)returnAddress);
            break;
        }

        currentFrame = *((void**)currentFrame);
        frameNumber++;
    }
}

void log_panic(const trap_frame_t* trapFrame, const char* format, ...)
{
    asm volatile("cli");

    if (atomic_exchange(&state.panicking, true))
    {
        while (true)
        {
            asm volatile("hlt");
        }
    }

    smp_halt_others();

    if (screen.initialized && !(state.config.outputs & LOG_OUTPUT_SCREEN))
    {
        log_screen_enable(NULL);
        state.config.outputs |= LOG_OUTPUT_SCREEN;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(state.panicBuffer, LOG_MAX_BUFFER, format, args);
    va_end(args);

    log_print(LOG_PANIC, "!!! KERNEL PANIC - %s !!!\n", state.panicBuffer);

    cpu_t* self = smp_self_unsafe();

    log_print(LOG_PANIC, "[SYSTEM STATE]\n");

    thread_t* currentThread = self->sched.runThread;
    if (currentThread == NULL)
    {
        log_print(LOG_PANIC, "thread: CPU=%d NULL THREAD\n", self->id);
    }
    else if (currentThread == self->sched.idleThread)
    {
        log_print(LOG_PANIC, "thread: CPU=%d IDLE\n", self->id);
    }
    else
    {
        log_print(LOG_PANIC, "thread: CPU=%d PID=%d TID=%d\n", self->id, currentThread->process->id, currentThread->id);
    }

    log_print(LOG_PANIC, "memory: free=%lluKB reserved=%lluKB\n", (pmm_free_amount() * PAGE_SIZE) / 1024,
        (pmm_reserved_amount() * PAGE_SIZE) / 1024);

    log_print(LOG_PANIC, "control regs: CR0=0x%016llx CR2=0x%016llx CR3=0x%016llx CR4=0x%016llx\n", cr0_read(),
        cr2_read(), cr3_read(), cr4_read());

    if (trapFrame)
    {
        log_panic_trap_frame(trapFrame);
        log_panic_trap_stack_trace(trapFrame);
    }
    else
    {
        log_panic_direct_stack_trace();
    }

    log_print(LOG_PANIC, "!!! KERNEL PANIC END - Please restart your machine !!!\n");

#ifdef QEMU_ISA_DEBUG_EXIT
    port_outb(QEMU_ISA_DEBUG_EXIT_PORT, EXIT_FAILURE);
#endif

    while (true)
    {
        asm volatile("hlt");
    }
}