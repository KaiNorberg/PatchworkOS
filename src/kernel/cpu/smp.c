#include "smp.h"

#include "acpi/tables.h"
#include "cpu/vectors.h"
#include "drivers/apic.h"
#include "drivers/time/hpet.h"
#include "kernel.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "trampoline.h"
#include "trap.h"

#include <common/defs.h>
#include <common/regs.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

static cpu_t bootstrapCpu;
static cpu_t* cpus[SMP_CPU_MAX];
static uint16_t cpuAmount = 0;
static bool isReady = false;

static atomic_uint16_t haltedAmount = ATOMIC_VAR_INIT(0);

static void cpu_init(cpu_t* cpu, uint8_t id, uint8_t lapicId, bool isBootstrap)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    cpu->isBootstrap = isBootstrap;
    tss_init(&cpu->tss);
    cli_ctx_init(&cpu->cli);
    sched_cpu_ctx_init(&cpu->sched, cpu);
    wait_cpu_ctx_init(&cpu->wait);
    statistics_cpu_ctx_init(&cpu->stat);
}

static void smp_entry(cpuid_t id)
{
    msr_write(MSR_CPU_ID, id);
    cpu_t* cpu = smp_self_unsafe();
    assert(cpu->id == id);

    kernel_other_init();

    trampoline_signal_ready(cpu->id);

    LOG_INFO("cpu %u with lapicid %u now idling\n", (uint64_t)cpu->id, (uint64_t)cpu->lapicId);
    sched_idle_loop();
}

static uint64_t cpu_start(cpu_t* cpu)
{
    assert(cpu->sched.idleThread != NULL);

    if (trampoline_cpu_setup(cpu->id, THREAD_KERNEL_STACK_TOP(cpu->sched.idleThread), smp_entry) != 0)
    {
        LOG_ERR("failed to setup trampoline for cpu %u\n", cpu->id);
        return -1;
    }

    lapic_send_init(cpu->lapicId);
    hpet_wait(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(cpu->lapicId, (void*)TRAMPOLINE_BASE_ADDR);

    if (trampoline_wait_ready(cpu->id, CLOCKS_PER_SEC) != 0)
    {
        LOG_ERR("cpu %d timed out\n", cpu->id);
        return -1;
    }

    return 0;
}

void smp_bootstrap_init(void)
{
    cpuAmount = 1;
    cpus[0] = &bootstrapCpu;
    cpu_init(cpus[0], 0, 0, true);

    msr_write(MSR_CPU_ID, cpus[0]->id);
}

void smp_others_init(void)
{
    trampoline_init();

    cpus[0]->lapicId = lapic_self_id();
    LOG_INFO("bootstrap cpu %u with lapicid %u, ready\n", (uint64_t)cpus[0]->id, (uint64_t)cpus[0]->lapicId);

    madt_t* madt = MADT_GET();

    madt_processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != MADT_INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC)
        {
            continue;
        }

        if (cpus[0]->lapicId == lapic->apicId)
        {
            continue;
        }

        if (lapic->flags & MADT_PROCESSOR_LOCAL_APIC_ENABLED)
        {
            cpuid_t newId = cpuAmount++;
            cpus[newId] = heap_alloc(sizeof(cpu_t), HEAP_NONE);
            if (cpus[newId] == NULL)
            {
                panic(NULL, "Failed to allocate memory for cpu %d with lapicid %d", (uint64_t)newId,
                    (uint64_t)lapic->apicId);
            }

            cpu_init(cpus[newId], newId, lapic->apicId, false);

            if (cpu_start(cpus[newId]) == ERR)
            {
                panic(NULL, "Failed to start cpu %d with lapicid %d", (uint64_t)cpus[newId]->id,
                    (uint64_t)cpus[newId]->lapicId);
            }
        }
    }

    trampoline_deinit();
}

static void smp_halt_ipi(trap_frame_t* trapFrame)
{
    atomic_fetch_add(&haltedAmount, 1);

    while (1)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}

void smp_halt_others(void)
{
    const cpu_t* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            lapic_send_ipi(cpus[id]->lapicId, VECTOR_HALT);
        }
    }
}

uint16_t smp_cpu_amount(void)
{
    return cpuAmount;
}

cpu_t* smp_cpu(cpuid_t id)
{
    if (id >= cpuAmount)
    {
        panic(NULL, "smp_cpu(): invalid cpu id %u\n", (uint64_t)id);
    }

    return cpus[id];
}

cpu_t* smp_self_unsafe(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    return cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self(void)
{
    cli_push();

    return cpus[msr_read(MSR_CPU_ID)];
}

void smp_put(void)
{
    cli_pop();
}
