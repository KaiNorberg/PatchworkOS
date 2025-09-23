#include "panic.h"

#include "cpu/port.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "log/log.h"
#include "log/screen.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sched/timer.h"

#include <boot/boot_info.h>
#include <common/regs.h>
#include <common/version.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
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
    LOG_PANIC("rip: %04llx:0x%016llx ", trapFrame->cs, trapFrame->rip);

    uintptr_t offset;
    const char* symbol = panic_resolve_symbol(trapFrame->rip, &offset);
    if (symbol)
    {
        LOG_PANIC("0x%s+0x%llx", symbol, offset);
    }
    LOG_PANIC("\n");

    LOG_PANIC("rsp: %04llx:0x%016llx rflags: 0x%08llx\n", trapFrame->ss, trapFrame->rsp,
        trapFrame->rflags & 0xFFFFFFFF);

    LOG_PANIC("rax: 0x%016llx rbx: 0x%016llx rcx: 0x%016llx\n", trapFrame->rax, trapFrame->rbx, trapFrame->rcx);
    LOG_PANIC("rdx: 0x%016llx rsi: 0x%016llx rdi: 0x%016llx\n", trapFrame->rdx, trapFrame->rsi, trapFrame->rdi);
    LOG_PANIC("rbp: 0x%016llx r08: 0x%016llx r09: 0x%016llx\n", trapFrame->rbp, trapFrame->r8, trapFrame->r9);
    LOG_PANIC("r10: 0x%016llx r11: 0x%016llx r12: 0x%016llx\n", trapFrame->r10, trapFrame->r11, trapFrame->r12);
    LOG_PANIC("r13: 0x%016llx r14: 0x%016llx r15: 0x%016llx\n", trapFrame->r13, trapFrame->r14, trapFrame->r15);
}

static void panic_stack_trace(const trap_frame_t* trapFrame)
{
    LOG_PANIC("stack trace:\n");

    uint64_t* rbp = (uint64_t*)trapFrame->rbp;
    uint64_t* prevFrame = NULL;

    uintptr_t offset;
    const char* symbol = panic_resolve_symbol(trapFrame->rip, &offset);
    if (symbol)
    {
        LOG_PANIC(" [0x%016llx] %s+0x%llx\n", trapFrame->rip, symbol, offset);
    }
    else
    {
        LOG_PANIC(" [0x%016llx]\n", trapFrame->rip);
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
                LOG_PANIC(" [0x%016llx] %s+0x%llx\n", returnAddress, symbol, offset);
            }
            else
            {
                LOG_PANIC(" [0x%016llx]\n", returnAddress);
            }
        }

        prevFrame = rbp;
        rbp = (uint64_t*)rbp[0];
    }
}

static void panic_direct_stack_trace(void)
{
    LOG_PANIC("stack trace:\n");

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
                LOG_PANIC(" [0x%016llx] %s+0x%llx\n", (uintptr_t)returnAddress, symbol, offset);
            }
            else
            {
                LOG_PANIC(" [0x%016llx]\n", (uintptr_t)returnAddress);
            }
        }

        prevFrame = currentFrame;
        currentFrame = *((void**)currentFrame);
    }
}

static bool panic_is_valid_address(uintptr_t addr)
{
    if (addr < PAGE_SIZE)
    {
        return false;
    }
    if (addr >= PML_HIGHER_HALF_END)
    {
        return false;
    }
    if (addr > PML_LOWER_HALF_END && addr < PML_LOWER_HALF_START)
    {
        return false;
    }
    return true;
}

static void panic_print_stack_dump(const trap_frame_t* trapFrame)
{
    uint64_t rsp = trapFrame->rsp;
    LOG_PANIC("stack dump (around rsp=0x%016llx):\n", rsp);

    const int linesToDump = 8;
    const int bytesPerLine = 16;
    uint64_t startaAddr = (rsp - (linesToDump / 2 - 1) * bytesPerLine) & ~(bytesPerLine - 1);

    for (int i = 0; i < linesToDump; i++)
    {
        uint64_t line_addr = startaAddr + (i * bytesPerLine);

        LOG_PANIC("  0x%016llx: ", line_addr);

        for (int j = 0; j < bytesPerLine; j++)
        {
            uintptr_t current_addr = line_addr + j;
            if (panic_is_valid_address(current_addr))
            {
                LOG_PANIC("%02x ", *(uint8_t*)current_addr);
            }
            else
            {
                LOG_PANIC("   ");
            }
        }

        LOG_PANIC("|");

        for (int j = 0; j < bytesPerLine; j++)
        {
            uintptr_t current_addr = line_addr + j;
            if (panic_is_valid_address(current_addr))
            {
                uint8_t c = *(uint8_t*)current_addr;
                LOG_PANIC("%c", (c >= 32 && c <= 126) ? c : '.');
            }
            else
            {
                LOG_PANIC(" ");
            }
        }
        LOG_PANIC("|\n");

        if (rsp >= line_addr && rsp < line_addr + bytesPerLine)
        {
            LOG_PANIC("                      ");

            uint64_t offset_in_line = rsp - line_addr;
            for (uint64_t k = 0; k < offset_in_line; k++)
            {
                LOG_PANIC("   ");
            }
            LOG_PANIC("^^ RSP\n");
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
        log_screen_enable();
        state->config.outputs |= LOG_OUTPUT_SCREEN;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(state->panicBuffer, LOG_MAX_BUFFER, format, args);
    va_end(args);

    LOG_PANIC("!!! KERNEL PANIC %s version %s !!!\n", OS_NAME, OS_VERSION);
    LOG_PANIC("cause: %s\n", state->panicBuffer);

    thread_t* currentThread = self->sched.runThread;
    if (currentThread == NULL)
    {
        LOG_PANIC("thread: cpu=%d null\n", self->id);
    }
    else if (currentThread == self->sched.idleThread)
    {
        LOG_PANIC("thread: cpu=%d idle\n", self->id);
    }
    else
    {
        LOG_PANIC("thread: cpu=%d pid=%d tid=%d\n", self->id, currentThread->process->id, currentThread->id);
    }

    errno_t lastError = currentThread->error;
    LOG_PANIC("last errno: %d (%s)\n", lastError, strerror(lastError));

    uint64_t freePages = pmm_free_amount();
    uint64_t reservedPages = pmm_reserved_amount();
    uint64_t totalPages = freePages + reservedPages;

    LOG_PANIC("memory: %lluK/%lluK available (%lluK kernel code/data, %lluK reserved)\n",
        (freePages * PAGE_SIZE) / 1024, (totalPages * PAGE_SIZE) / 1024,
        (((uintptr_t)&_kernelEnd - (uintptr_t)&_kernelStart) / 1024), (reservedPages * PAGE_SIZE) / 1024);

    uint64_t cr0 = cr0_read();
    uint64_t cr2 = cr2_read();
    uint64_t cr3 = cr3_read();
    uint64_t cr4 = cr4_read();

    LOG_PANIC("\ncr0=0x%016llx cr2=0x%016llx cr3=0x%016llx cr4=0x%016llx\n", cr0, cr2, cr3, cr4);
    LOG_PANIC("cr0 flags:");
    if (cr0 & CR0_PROTECTED_MODE_ENABLE)
    {
        LOG_PANIC(" pe");
    }
    if (cr0 & CR0_MONITOR_CO_PROCESSOR)
    {
        LOG_PANIC(" mp");
    }
    if (cr0 & CR0_EMULATION)
    {
        LOG_PANIC(" em");
    }
    if (cr0 & CR0_TASK_SWITCHED)
    {
        LOG_PANIC(" ts");
    }
    if (cr0 & CR0_EXTENSION_TYPE)
    {
        LOG_PANIC(" et");
    }
    if (cr0 & CR0_NUMERIC_ERROR_ENABLE)
    {
        LOG_PANIC(" ne");
    }
    if (cr0 & CR0_WRITE_PROTECT)
    {
        LOG_PANIC(" wp");
    }
    if (cr0 & CR0_ALIGNMENT_MASK)
    {
        LOG_PANIC(" am");
    }
    if (cr0 & CR0_NOT_WRITE_THROUGH)
    {
        LOG_PANIC(" nw");
    }
    if (cr0 & CR0_CACHE_DISABLE)
    {
        LOG_PANIC(" cd");
    }
    if (cr0 & CR0_PAGING_ENABLE)
    {
        LOG_PANIC(" pg");
    }
    LOG_PANIC("\n");

    if (trapFrame != NULL)
    {
        LOG_PANIC("exception: %s (vector: %lld, error code: 0x%llx)\n", panic_get_exception_name(trapFrame->vector),
            trapFrame->vector, trapFrame->errorCode);

        if (trapFrame->vector == VECTOR_PAGE_FAULT)
        {
            LOG_PANIC("page fault details: A %s operation to a %s page caused a %s.\n",
                (trapFrame->errorCode & 2) ? "write" : "read", (trapFrame->errorCode & 4) ? "user-mode" : "kernel-mode",
                (trapFrame->errorCode & 1) ? "protection violation" : "non-present page fault");
            if (trapFrame->errorCode & 8)
            {
                LOG_PANIC("                      (Reserved bit violation)\n");
            }
            if (trapFrame->errorCode & 16)
            {
                LOG_PANIC("                       (Instruction fetch)\n");
            }
        }
    }

    LOG_PANIC("\n");

    if (trapFrame != NULL)
    {
        panic_registers(trapFrame);

        LOG_PANIC("\n");
        panic_print_stack_dump(trapFrame);
        LOG_PANIC("\n");
        panic_stack_trace(trapFrame);
    }
    else
    {
        panic_direct_stack_trace();
    }

    LOG_PANIC("!!! Please restart your machine !!!\n");

#ifdef QEMU_ISA_DEBUG_EXIT
    port_outb(QEMU_ISA_DEBUG_EXIT_PORT, EXIT_FAILURE);
#endif

    while (true)
    {
        asm volatile("hlt");
    }
}
