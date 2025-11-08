#include "trampoline.h"

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/drivers/apic.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/utils/utils.h>

#include <kernel/mem/paging_types.h>
#include <string.h>

static void* backupBuffer;
static void* trampolineStack;

static atomic_bool cpuReadyFlag = ATOMIC_VAR_INIT(false);

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

    if (vmm_map(NULL, (void*)TRAMPOLINE_BASE_ADDR, (void*)TRAMPOLINE_BASE_ADDR, PAGE_SIZE,
            PML_WRITE | PML_PRESENT, NULL, NULL) == NULL)
    {
        panic(NULL, "Failed to map trampoline");
    }

    uint8_t* virtBase = (uint8_t*)PML_LOWER_TO_HIGHER(TRAMPOLINE_BASE_ADDR);
    memcpy(backupBuffer, virtBase, TRAMPOLINE_SIZE);
    memcpy(virtBase, trampoline_start, TRAMPOLINE_SIZE);
    memset(&virtBase[TRAMPOLINE_DATA_OFFSET], 0, PAGE_SIZE - TRAMPOLINE_DATA_OFFSET);

    WRITE_64(&virtBase[TRAMPOLINE_PML4_OFFSET], PML_ENSURE_LOWER_HALF(vmm_get_kernel_space()->pageTable.pml4));
    WRITE_64(&virtBase[TRAMPOLINE_ENTRY_OFFSET], (uintptr_t)trampoline_c_entry);

    atomic_init(&cpuReadyFlag, false);

    LOG_DEBUG("trampoline initialized\n");
}

void trampoline_deinit(void)
{
    uint8_t* virtBase = (uint8_t*)PML_LOWER_TO_HIGHER(TRAMPOLINE_BASE_ADDR);
    memcpy(virtBase, backupBuffer, PAGE_SIZE);

    vmm_unmap(NULL, (void*)TRAMPOLINE_BASE_ADDR, PAGE_SIZE);

    pmm_free(backupBuffer);
    backupBuffer = NULL;
    pmm_free(trampolineStack);
    trampolineStack = NULL;

    LOG_DEBUG("trampoline deinitialized\n");
}

void trampoline_send_startup_ipi(cpu_t* cpu, lapic_id_t lapicId)
{
    uint8_t* virtBase = (uint8_t*)PML_LOWER_TO_HIGHER(TRAMPOLINE_BASE_ADDR);
    WRITE_64(&virtBase[TRAMPOLINE_CPU_OFFSET], (uintptr_t)cpu);
    WRITE_64(&virtBase[TRAMPOLINE_STACK_OFFSET], (uintptr_t)trampolineStack + PAGE_SIZE);
    
    atomic_store(&cpuReadyFlag, false);

    lapic_send_init(lapicId);
    sys_time_wait(CLOCKS_PER_SEC / 100);
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

        sys_time_wait(CLOCKS_PER_SEC / 10000);
        elapsed += CLOCKS_PER_SEC / 10000;
    }

    return ERR;
}

static void trampoline_after_jump(void)
{
    atomic_store(&cpuReadyFlag, true);
    sched_idle_loop();
}

void trampoline_c_entry(cpu_t* cpu)
{
    cpu_identify(cpu);

    if (cpu_init(cpu) == ERR)
    {
        panic(NULL, "Failed to initialize CPU%u", cpu->id);
    }

    thread_t* thread = sched_thread_unsafe();
    assert(thread != NULL);
    assert(sched_is_idle(cpu));
    thread->frame.rip = (uintptr_t)trampoline_after_jump;
    thread->frame.rsp = thread->kernelStack.top;
    thread->frame.cs = GDT_CS_RING0;
    thread->frame.ss = GDT_SS_RING0;
    thread->frame.rflags = RFLAGS_ALWAYS_SET;
    thread_jump(thread);
}
