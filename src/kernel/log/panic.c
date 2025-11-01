#include <kernel/log/panic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/port.h>
#include <kernel/cpu/regs.h>
#include <kernel/cpu/smp.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/symbol.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/version.h>

#include <boot/boot_info.h>

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

static char panicBuffer[LOG_MAX_BUFFER] = {0};

static atomic_uint32_t panicCpudId = ATOMIC_VAR_INIT(PANIC_NO_CPU_ID);

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

static void panic_registers(const interrupt_frame_t* frame)
{
    LOG_PANIC("rip: %04llx:0x%016llx ", frame->cs, frame->rip);

    symbol_info_t symbol;
    if (symbol_resolve_addr(&symbol, (void*)frame->rip) != ERR)
    {
        LOG_PANIC("<%s+0x%llx>", symbol.name, frame->rip - (uintptr_t)symbol.addr);
    }
    LOG_PANIC("\n");

    LOG_PANIC("rsp: %04llx:0x%016llx rflags: 0x%08llx\n", frame->ss, frame->rsp, frame->rflags & 0xFFFFFFFF);

    LOG_PANIC("rax: 0x%016llx rbx: 0x%016llx rcx: 0x%016llx\n", frame->rax, frame->rbx, frame->rcx);
    LOG_PANIC("rdx: 0x%016llx rsi: 0x%016llx rdi: 0x%016llx\n", frame->rdx, frame->rsi, frame->rdi);
    LOG_PANIC("rbp: 0x%016llx r08: 0x%016llx r09: 0x%016llx\n", frame->rbp, frame->r8, frame->r9);
    LOG_PANIC("r10: 0x%016llx r11: 0x%016llx r12: 0x%016llx\n", frame->r10, frame->r11, frame->r12);
    LOG_PANIC("r13: 0x%016llx r14: 0x%016llx r15: 0x%016llx\n", frame->r13, frame->r14, frame->r15);
}

static bool panic_is_valid_address(uintptr_t addr)
{
    if (addr == 0)
    {
        return false;
    }
    if (addr < VMM_USER_SPACE_MIN)
    {
        return false;
    }
    if (addr >= VMM_KERNEL_BINARY_MAX)
    {
        return false;
    }
    if (addr > VMM_USER_SPACE_MAX && addr < VMM_IDENTITY_MAPPED_MIN)
    {
        return false;
    }
    if (addr >= UINTPTR_MAX - sizeof(uint64_t) * 2)
    {
        return false;
    }
    if (addr >= VMM_KERNEL_STACKS_MAX - sizeof(uint64_t) * 4 && addr <= VMM_KERNEL_STACKS_MAX)
    {
        return false;
    }
    return true;
}

static bool panic_is_valid_stack_frame(uintptr_t ptr)
{
    if (ptr == 0)
    {
        return false;
    }
    if (ptr & 0x7)
    {
        return false;
    }
    if (!panic_is_valid_address(ptr))
    {
        return false;
    }
    if (!panic_is_valid_address(ptr + sizeof(uintptr_t)))
    {
        return false;
    }

    return true;
}

static void panic_print_stack_dump(const interrupt_frame_t* frame)
{
    uint64_t rsp = frame->rsp;
    LOG_PANIC("stack dump (around rsp=0x%016llx):\n", rsp);

    const int linesToDump = 8;
    const int bytesPerLine = 16;
    uint64_t startaAddr = (rsp - (linesToDump / 2 - 1) * bytesPerLine) & ~(bytesPerLine - 1);

    for (int i = 0; i < linesToDump; i++)
    {
        uint64_t lineAddr = startaAddr + (i * bytesPerLine);

        LOG_PANIC("  0x%016llx: ", lineAddr);

        for (int j = 0; j < bytesPerLine; j++)
        {
            uintptr_t currentAddr = lineAddr + j;
            if (panic_is_valid_address(currentAddr))
            {
                LOG_PANIC("%02x ", *(uint8_t*)currentAddr);
            }
            else
            {
                LOG_PANIC("   ");
            }
        }

        LOG_PANIC("|");

        for (int j = 0; j < bytesPerLine; j++)
        {
            uintptr_t currentAddr = lineAddr + j;
            if (panic_is_valid_address(currentAddr))
            {
                uint8_t c = *(uint8_t*)currentAddr;
                LOG_PANIC("%c", (c >= 32 && c <= 126) ? c : '.');
            }
            else
            {
                LOG_PANIC(" ");
            }
        }
        LOG_PANIC("|\n");

        if (rsp >= lineAddr && rsp < lineAddr + bytesPerLine)
        {
            LOG_PANIC("                      ");

            uint64_t offsetInLine = rsp - lineAddr;
            for (uint64_t k = 0; k < offsetInLine; k++)
            {
                LOG_PANIC("   ");
            }
            LOG_PANIC("^^ RSP\n");
        }
    }
}

static uint64_t panic_print_trace_address(uintptr_t addr)
{
    if (addr >= VMM_USER_SPACE_MIN && addr < VMM_USER_SPACE_MAX)
    {
        LOG_PANIC("  [0x%016llx] <user space address>\n", addr);
        return ERR;
    }

    symbol_info_t symbol;
    if (symbol_resolve_addr(&symbol, (void*)addr) == ERR)
    {
        LOG_PANIC("  [0x%016llx] <unknown>\n", addr);
        return ERR;
    }

    LOG_PANIC("  [0x%016llx] <%s+0x%llx>\n", addr, symbol.name, addr - (uintptr_t)symbol.addr);
    return 0;
}

static void panic_unwind_stack(uintptr_t* rbp)
{
    LOG_PANIC("stack trace:\n");
    uintptr_t* prevFrame = NULL;
    uint64_t depth = 0;

    while (rbp != NULL && rbp != prevFrame)
    {
        if (depth >= PANIC_MAX_STACK_FRAMES)
        {
            LOG_PANIC("  ...\n");
            break;
        }

        if (!panic_is_valid_stack_frame((uintptr_t)rbp))
        {
            LOG_PANIC("  [0x%016llx] <invalid frame pointer>\n", (uintptr_t)rbp);
            break;
        }

        uintptr_t returnAddress = rbp[1];
        if (returnAddress == 0)
        {
            LOG_PANIC("  [0x%016llx] <null return address>\n", returnAddress);
            break;
        }

        if (panic_print_trace_address(returnAddress) == ERR)
        {
            LOG_PANIC("  [0x%016llx] <failed to resolve>\n", returnAddress);
            break;
        }

        prevFrame = rbp;
        rbp = (uintptr_t*)rbp[0];
        depth++;
    }
}

static void panic_direct_stack_trace(void)
{
    panic_unwind_stack(__builtin_frame_address(0));
}

void panic_stack_trace(const interrupt_frame_t* frame)
{
    panic_unwind_stack((uintptr_t*)frame->rbp);
}

void panic(const interrupt_frame_t* frame, const char* format, ...)
{
    asm volatile("cli");

    cpuid_t selfId = smp_self_id_unsafe();
    uint32_t expectedCpuId = PANIC_NO_CPU_ID;
    if (!atomic_compare_exchange_strong(&panicCpudId, &expectedCpuId, selfId))
    {
        if (expectedCpuId == selfId)
        {
            // Print basic message for double panic on same CPU but avoid using the full panic stuff again.
            const char* message = "!!! KERNEL DOUBLE PANIC ON SAME CPU !!!\n";
            log_write(message, strlen(message));
        }
        while (true)
        {
            asm volatile("hlt");
        }
    }

    va_list args;
    va_start(args, format);
    vsnprintf(panicBuffer, sizeof(panicBuffer), format, args);
    va_end(args);

    smp_halt_others();

    log_screen_enable();

    LOG_PANIC("!!! KERNEL PANIC (%s version %s) !!!\n", OS_NAME, OS_VERSION);
    LOG_PANIC("cause: %s\n", panicBuffer); // Filled in by panic()

    cpu_t* self = smp_self_unsafe();
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
    uint64_t reservedPages = pmm_used_amount();
    uint64_t totalPages = freePages + reservedPages;

    LOG_PANIC("memory: %lluK/%lluK available (%lluK kernel code/data, %lluK reserved)\n",
        (freePages * PAGE_SIZE) / 1024, (totalPages * PAGE_SIZE) / 1024,
        (((uintptr_t)&_kernelEnd - (uintptr_t)&_kernelStart) / 1024), (reservedPages * PAGE_SIZE) / 1024);

    uint64_t cr0 = cr0_read();
    uint64_t cr2 = cr2_read();
    uint64_t cr3 = cr3_read();
    uint64_t cr4 = cr4_read();

    LOG_PANIC("cr0=0x%016llx cr2=0x%016llx cr3=0x%016llx cr4=0x%016llx\n", cr0, cr2, cr3, cr4);
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

    if (frame != NULL)
    {
        LOG_PANIC("exception: %s (vector: %lld, error code: 0x%llx)\n", panic_get_exception_name(frame->vector),
            frame->vector, frame->errorCode);

        if (frame->vector == EXCEPTION_PAGE_FAULT)
        {
            LOG_PANIC("page fault details: A %s operation to a %s page caused a %s.\n",
                (frame->errorCode & 2) ? "write" : "read", (frame->errorCode & 4) ? "user-mode" : "kernel-mode",
                (frame->errorCode & 1) ? "protection violation" : "non-present page fault");
            if (frame->errorCode & 8)
            {
                LOG_PANIC("                    (Reserved bit violation)\n");
            }
            if (frame->errorCode & 16)
            {
                LOG_PANIC("                    (Instruction fetch)\n");
            }

            if (cr2 == 0)
            {
                LOG_PANIC("                    (Faulting address is NULL)\n");
            }
            else if (cr2 >= VMM_KERNEL_BINARY_MIN && cr2 < VMM_KERNEL_BINARY_MAX)
            {
                LOG_PANIC("                    (Faulting address is in kernel binary region)\n");
            }
            else if (cr2 >= VMM_KERNEL_HEAP_MIN && cr2 < VMM_KERNEL_HEAP_MAX)
            {
                LOG_PANIC("                    (Faulting address is in kernel heap region)\n");
            }
            else if (cr2 >= VMM_KERNEL_STACKS_MIN && cr2 < VMM_KERNEL_STACKS_MAX)
            {
                LOG_PANIC("                    (Faulting address is in kernel stacks region)\n");
            }
        }
    }

    if (frame != NULL)
    {
        panic_registers(frame);
        panic_print_stack_dump(frame);
        panic_stack_trace(frame);
    }
    else
    {
        panic_direct_stack_trace();
    }

    LOG_PANIC("!!! Please restart your machine !!!\n");

#ifdef QEMU_EXIT_ON_PANIC
    port_outb(QEMU_EXIT_ON_PANIC_PORT, EXIT_FAILURE);
#endif

    while (true)
    {
        asm volatile("hlt");
    }
}
