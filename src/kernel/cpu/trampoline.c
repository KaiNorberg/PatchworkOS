#include "trampoline.h"

#include "cpu.h"
#include "drivers/apic.h"
#include "drivers/hpet.h"
#include "gdt.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "utils/utils.h"

#include <common/paging_types.h>
#include <string.h>

static void* backupBuffer;
static void* trampolineStack;

static atomic_bool cpuReadyFlag;

void trampoline_init(void)
{
    backupBuffer = pmm_alloc();
    if (backupBuffer == NULL)
    {
        panic(NULL, "Failed to allocate memory for trampoline backup");
    }

    trampolineStack = pmm_alloc();
    if (trampolineStack == NULL)
    {
        panic(NULL, "Failed to allocate memory for trampoline stack");
    }

    assert(TRAMPOLINE_SIZE < PAGE_SIZE);

    memcpy(backupBuffer, (void*)TRAMPOLINE_BASE_ADDR, TRAMPOLINE_SIZE);
    memcpy((void*)TRAMPOLINE_BASE_ADDR, trampoline_start, TRAMPOLINE_SIZE);
    memset(TRAMPOLINE_ADDR(TRAMPOLINE_DATA_OFFSET), 0, PAGE_SIZE - TRAMPOLINE_DATA_OFFSET);

    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_PML4_OFFSET), PML_ENSURE_LOWER_HALF(vmm_get_kernel_space()->pageTable.pml4));
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET), (uintptr_t)trampoline_c_entry);

    atomic_init(&cpuReadyFlag, false);

    LOG_DEBUG("trampoline initialized\n");
}

void trampoline_deinit(void)
{
    memcpy((void*)TRAMPOLINE_BASE_ADDR, backupBuffer, PAGE_SIZE);

    pmm_free(backupBuffer);
    backupBuffer = NULL;
    pmm_free(trampolineStack);
    trampolineStack = NULL;

    LOG_DEBUG("trampoline deinitialized\n");
}

void trampoline_send_startup_ipi(cpu_t* cpu, cpuid_t id, lapic_id_t lapicId)
{
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET), id);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_CPU_OFFSET), (uintptr_t)cpu);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), (uintptr_t)trampolineStack + PAGE_SIZE);
    atomic_store(&cpuReadyFlag, false);

    lapic_send_init(lapicId);
    hpet_wait(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(lapicId, (void*)TRAMPOLINE_BASE_ADDR);
}

uint64_t trampoline_wait_ready(clock_t timeout)
{
    clock_t elapsed = 0;

    while (elapsed < timeout)
    {
        if (atomic_load(&cpuReadyFlag))
        {
            return 0;
        }

        hpet_wait(CLOCKS_PER_SEC / 10000);
        elapsed += CLOCKS_PER_SEC / 10000;
    }

    return ERR;
}

static void trampoline_after_jump(void)
{
    atomic_store(&cpuReadyFlag, true);
    sched_idle_loop();
}

void trampoline_c_entry(cpu_t* cpu, cpuid_t cpuId)
{
    if (cpu_init(cpu, cpuId) == ERR)
    {
        panic(NULL, "Failed to initialize CPU %u", cpuId);
    }

    thread_t* thread = sched_thread();
    assert(thread != NULL);
    assert(sched_is_idle());
    thread->frame.rip = (uintptr_t)trampoline_after_jump;
    thread->frame.rsp = thread->kernelStack.top;
    thread->frame.cs = GDT_CS_RING0;
    thread->frame.ss = GDT_SS_RING0;
    thread->frame.rflags = RFLAGS_ALWAYS_SET;
    thread_jump(thread);
}
