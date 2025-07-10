#include "panic.h"

#include "cpu/port.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "defs.h"
#include "drivers/com.h"
#include "drivers/systime/systime.h"
#include "log/log.h"
#include "log/screen.h"
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

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

static bool panic_is_valid_stack_frame(void* ptr)
{
    if (ptr == NULL)
    {
        return false;
    }
    if ((uintptr_t)ptr & 0x7)
    {
        return false;
    }
    if ((uintptr_t)ptr < PAGE_SIZE)
    {
        return false;
    }
    if ((uintptr_t)ptr >= PML_HIGHER_HALF_END)
    {
        return false;
    }
    if ((uintptr_t)ptr > PML_LOWER_HALF_END && (uintptr_t)ptr < PML_LOWER_HALF_START)
    {
        return false;
    }
    if ((uintptr_t)ptr > UINTPTR_MAX - 16)
    {
        return false;
    }

    return true;
}

static bool panic_is_valid_return_address(void* ptr)
{
    if (ptr == NULL)
    {
        return false;
    }

    if ((uintptr_t)ptr >= (uintptr_t)&_kernelStart && (uintptr_t)ptr < (uintptr_t)&_kernelEnd)
    {
        return true;
    }

    return false;
}

static const char* panic_resolve_symbol(uintptr_t addr, uintptr_t* offset)
{
    // TODO: Implement symbol resolution here after bootloader overhaul.
    *offset = 0;
    return NULL;
}

static const char* panic_get_exception_name(uint64_t vector)
{
    static const char* names[] = {
        "divide error",
        "debug",
        "nmi",
        "breakpoint",
        "overflow",
        "bound range exceeded",
        "invalid opcode",
        "device not available",
        "double fault",
        "coprocessor segment overrun",
        "invalid TSS",
        "segment not present",
        "stack fault",
        "general protection",
        "page fault",
        "reserved",
        "x87 FPU error",
        "alignment check",
        "machine check",
        "SIMD exception",
        "virtualization exception",
        "control protection exception",
    };

    if (vector < sizeof(names) / sizeof(names[0]))
    {
        return names[vector];
    }
    return "unknown exception";
}

static void panic_registers(const trap_frame_t* trapFrame)
{
    log_print(LOG_LEVEL_PANIC, "rip: %04llx:0x%016llx ", trapFrame->cs, trapFrame->rip);

    uintptr_t offset;
    const char* symbol = panic_resolve_symbol(trapFrame->rip, &offset);
    if (symbol)
    {
        log_print(LOG_LEVEL_PANIC, "0x%s+0x%llx", symbol, offset);
    }
    log_print(LOG_LEVEL_PANIC, "\n");

    log_print(LOG_LEVEL_PANIC, "rsp: %04llx:0x%016llx rflags: 0x%08llx\n", trapFrame->ss, trapFrame->rsp,
        trapFrame->rflags & 0xFFFFFFFF);

    log_print(LOG_LEVEL_PANIC, "rax: 0x%016llx rbx: 0x%016llx rcx: 0x%016llx\n", trapFrame->rax, trapFrame->rbx,
        trapFrame->rcx);
    log_print(LOG_LEVEL_PANIC, "rdx: 0x%016llx rsi: 0x%016llx rdi: 0x%016llx\n", trapFrame->rdx, trapFrame->rsi,
        trapFrame->rdi);
    log_print(LOG_LEVEL_PANIC, "rbp: 0x%016llx r08: 0x%016llx r09: 0x%016llx\n", trapFrame->rbp, trapFrame->r8,
        trapFrame->r9);
    log_print(LOG_LEVEL_PANIC, "r10: 0x%016llx r11: 0x%016llx r12: 0x%016llx\n", trapFrame->r10, trapFrame->r11,
        trapFrame->r12);
    log_print(LOG_LEVEL_PANIC, "r13: 0x%016llx r14: 0x%016llx r15: 0x%016llx\n", trapFrame->r13, trapFrame->r14,
        trapFrame->r15);
}

static void panic_stack_trace(const trap_frame_t* trapFrame)
{
    log_print(LOG_LEVEL_PANIC, "stack trace:\n");

    uint64_t* rbp = (uint64_t*)trapFrame->rbp;
    uint64_t* prevFrame = NULL;

    uintptr_t offset;
    const char* symbol = panic_resolve_symbol(trapFrame->rip, &offset);
    if (symbol)
    {
        log_print(LOG_LEVEL_PANIC, " [0x%016llx] %s+0x%llx\n", trapFrame->rip, symbol, offset);
    }
    else
    {
        log_print(LOG_LEVEL_PANIC, " [0x%016llx]\n", trapFrame->rip);
    }

    while (rbp != NULL && rbp != prevFrame)
    {
        if (!panic_is_valid_stack_frame(rbp))
        {
            break;
        }

        if (prevFrame && rbp <= prevFrame)
        {
            break;
        }

        uint64_t returnAddress = rbp[1];
        if (returnAddress == 0)
        {
            break;
        }

        if (panic_is_valid_return_address((void*)returnAddress))
        {
            symbol = panic_resolve_symbol(returnAddress, &offset);
            if (symbol)
            {
                log_print(LOG_LEVEL_PANIC, " [0x%016llx] %s+0x%llx\n", returnAddress, symbol, offset);
            }
            else
            {
                log_print(LOG_LEVEL_PANIC, " [0x%016llx]\n", returnAddress);
            }
        }

        prevFrame = rbp;
        rbp = (uint64_t*)rbp[0];
    }
}

static void panic_direct_stack_trace(void)
{
    log_print(LOG_LEVEL_PANIC, "stack trace:\n");

    void* currentFrame = __builtin_frame_address(0);
    void* prevFrame = NULL;

    while (currentFrame != NULL && currentFrame != prevFrame)
    {
        if (!panic_is_valid_stack_frame(currentFrame))
        {
            break;
        }

        void* returnAddress = *((void**)currentFrame + 1);
        if (returnAddress == NULL)
        {
            break;
        }

        if (panic_is_valid_return_address(returnAddress))
        {
            uintptr_t offset;
            const char* symbol = panic_resolve_symbol((uintptr_t)returnAddress, &offset);
            if (symbol)
            {
                log_print(LOG_LEVEL_PANIC, " [0x%016llx] %s+0x%llx\n", (uintptr_t)returnAddress, symbol, offset);
            }
            else
            {
                log_print(LOG_LEVEL_PANIC, " [0x%016llx]\n", (uintptr_t)returnAddress);
            }
        }

        prevFrame = currentFrame;
        currentFrame = *((void**)currentFrame);
    }
}

static void panic_memory_context(const trap_frame_t* trapFrame)
{
    uint64_t rsp = trapFrame->rsp;
    log_print(LOG_LEVEL_PANIC, "stack content (0x%016llx):\n", rsp);

    for (int i = -4; i <= 4; i++)
    {
        uint64_t addr = rsp + (i * 8);
        if (panic_is_valid_stack_frame((void*)addr))
        {
            uint64_t value = *(uint64_t*)addr;
            log_print(LOG_LEVEL_PANIC, "  [rsp%+3d] 0x%016llx: 0x%016llx", i * 8, addr, value);

            if (panic_is_valid_return_address((void*)value))
            {
                uintptr_t offset;
                const char* symbol = panic_resolve_symbol(value, &offset);
                if (symbol)
                {
                    log_print(LOG_LEVEL_PANIC, " <%s+0x%llx>", symbol, offset);
                }
            }
            log_print(LOG_LEVEL_PANIC, "\n");
        }
    }
}

void panic(const trap_frame_t* trapFrame, const char* format, ...)
{
    screen_t* screen = log_get_screen();
    log_state_t* state = log_get_state();

    asm volatile("cli");

    cpu_t* self = smp_self_unsafe();

    uint32_t expectedCpuId = LOG_NO_PANIC_CPU_ID;
    if (!atomic_compare_exchange_strong(&state->panickingCpuId, &expectedCpuId, self->id))
    {
        if (expectedCpuId == self->id)
        {
            const char* message = "!!! KERNEL DOUBLE PANIC ON SAME CPU !!!\n";
            log_write(message, strlen(message));
        }
        while (true)
        {
            asm volatile("hlt");
        }
    }

    smp_halt_others();

    if (screen->initialized && !(state->config.outputs & LOG_OUTPUT_SCREEN))
    {
        log_screen_enable(NULL);
        state->config.outputs |= LOG_OUTPUT_SCREEN;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(state->panicBuffer, LOG_MAX_BUFFER, format, args);
    va_end(args);

    log_print(LOG_LEVEL_PANIC, "!!! KERNEL PANIC - %s !!!\n", state->panicBuffer);

    thread_t* currentThread = self->sched.runThread;
    if (currentThread == NULL)
    {
        log_print(LOG_LEVEL_PANIC, "thread: cpu=%d null\n", self->id);
    }
    else if (currentThread == self->sched.idleThread)
    {
        log_print(LOG_LEVEL_PANIC, "thread: cpu=%d idle\n", self->id);
    }
    else
    {
        log_print(LOG_LEVEL_PANIC, "thread: cpu=%d pid=%d tid=%d\n", self->id, currentThread->process->id,
            currentThread->id);
    }

    uint64_t freePages = pmm_free_amount();
    uint64_t reservedPages = pmm_reserved_amount();
    uint64_t totalPages = freePages + reservedPages;

    log_print(LOG_LEVEL_PANIC, "memory: %lluK/%lluK available (%lluK kernel code/data, %lluK reserved)\n",
        (freePages * PAGE_SIZE) / 1024,
        (totalPages * PAGE_SIZE) / 1024,
        (((uintptr_t)&_kernelEnd - (uintptr_t)&_kernelStart) / 1024),
        (reservedPages * PAGE_SIZE) / 1024);

    uint64_t cr0 = cr0_read();
    uint64_t cr2 = cr2_read();
    uint64_t cr3 = cr3_read();
    uint64_t cr4 = cr4_read();

    log_print(LOG_LEVEL_PANIC, "control regs: CR0=0x%016llx CR2=0x%016llx CR3=0x%016llx CR4=0x%016llx\n", cr0, cr2, cr3,
        cr4);

    log_print(LOG_LEVEL_PANIC, "cr0 flags:");
    if (cr0 & (1 << 0))
    {
        log_print(LOG_LEVEL_PANIC, " pe");
    }
    if (cr0 & (1 << 1))
    {
        log_print(LOG_LEVEL_PANIC, " mp");
    }
    if (cr0 & (1 << 2))
    {
        log_print(LOG_LEVEL_PANIC, " em");
    }
    if (cr0 & (1 << 3))
    {
        log_print(LOG_LEVEL_PANIC, " ts");
    }
    if (cr0 & (1 << 4))
    {
        log_print(LOG_LEVEL_PANIC, " et");
    }
    if (cr0 & (1 << 5))
    {
        log_print(LOG_LEVEL_PANIC, " ne");
    }
    if (cr0 & (1 << 16))
    {
        log_print(LOG_LEVEL_PANIC, " wp");
    }
    if (cr0 & (1 << 18))
    {
        log_print(LOG_LEVEL_PANIC, " am");
    }
    if (cr0 & (1 << 29))
    {
        log_print(LOG_LEVEL_PANIC, " nw");
    }
    if (cr0 & (1 << 30))
    {
        log_print(LOG_LEVEL_PANIC, " cd");
    }
    if (cr0 & (1 << 31))
    {
        log_print(LOG_LEVEL_PANIC, " pg");
    }
    log_print(LOG_LEVEL_PANIC, "\n");

    if (trapFrame != NULL && trapFrame->vector == VECTOR_PAGE_FAULT)
    {
        log_print(LOG_LEVEL_PANIC, "page fault: %s %s %s%s\n",
            (trapFrame->errorCode & 1) ? "protection violation" : "page not present",
            (trapFrame->errorCode & 2) ? "write" : "read", (trapFrame->errorCode & 4) ? "user" : "kernel",
            (trapFrame->errorCode & 8) ? " reserved bit violation" : "");
    }

    log_print(LOG_LEVEL_PANIC, "\n");

    if (trapFrame != NULL)
    {
        panic_registers(trapFrame);

        log_print(LOG_LEVEL_PANIC, "\n");
        panic_memory_context(trapFrame);
        panic_stack_trace(trapFrame);
    }
    else
    {
        panic_direct_stack_trace();
    }

    log_print(LOG_LEVEL_PANIC, "!!! KERNEL PANIC END - Please restart your machine !!!\n");

#ifdef QEMU_ISA_DEBUG_EXIT
    port_outb(QEMU_ISA_DEBUG_EXIT_PORT, EXIT_FAILURE);
#endif

    while (true)
    {
        asm volatile("hlt");
    }
}
