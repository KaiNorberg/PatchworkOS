#include "smp.h"

#include "acpi/madt.h"
#include "apic.h"
#include "boot/kernel.h"
#include "drivers/systime/hpet.h"
#include "gdt.h"
#include "idt.h"
#include "mem/vmm.h"
#include "regs.h"
#include "sched/sched.h"
#include "sync/futex.h"
#include "sync/lock.h"
#include "trampoline.h"
#include "trap.h"
#include "utils/log.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cpu_t bootstrapCpu;
static cpu_t* cpus[CPU_MAX_AMOUNT];
static uint8_t cpuAmount = 0;
static bool cpuReady = false;

atomic_uint8 haltedAmount = ATOMIC_VAR_INIT(0);

static void ipi_queue_init(ipi_queue_t* queue)
{
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

static void ipi_queue_push(ipi_queue_t* queue, ipi_t ipi)
{
    LOCK_DEFER(&queue->lock);
    queue->ipis[queue->writeIndex] = ipi;
    queue->writeIndex = (queue->writeIndex + 1) % IPI_QUEUE_MAX;
}

static ipi_t ipi_queue_pop(ipi_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    if (queue->writeIndex == queue->readIndex)
    {
        return NULL;
    }

    ipi_t ipi = queue->ipis[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % IPI_QUEUE_MAX;
    return ipi;
}

static void cpu_init(cpu_t* cpu, uint8_t id, uint8_t lapicId)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    cpu->inSyscall = false;
    tss_init(&cpu->tss);
    cli_ctx_init(&cpu->cli);
    sched_ctx_init(&cpu->sched);
    wait_cpu_ctx_init(&cpu->wait);
    statistics_cpu_ctx_init(&cpu->stat);
    ipi_queue_init(&cpu->queue);
}

static uint64_t cpu_start(cpu_t* cpu)
{
    cpuReady = false;

    trampoline_cpu_setup(cpu);

    lapic_send_init(cpu->lapicId);
    hpet_sleep(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(cpu->lapicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    clock_t timeout = CLOCKS_PER_SEC * 100;
    while (!cpuReady)
    {
        clock_t sleepDuration = CLOCKS_PER_SEC / 1000;
        hpet_sleep(sleepDuration);
        timeout -= sleepDuration;
        if (timeout == 0)
        {
            return ERR;
        }
    }

    return 0;
}

void smp_bootstrap_init(void)
{
    cpuAmount = 1;
    cpus[0] = &bootstrapCpu;
    cpu_init(cpus[0], 0, 0);

    msr_write(MSR_CPU_ID, cpus[0]->id);
}

void smp_others_init(void)
{
    trampoline_init();

    uint8_t newId = 1;
    uint8_t lapicId = lapic_id();

    cpus[0]->lapicId = lapicId;
    printf("cpu %d: bootstrap cpu, ready\n", (uint64_t)cpus[0]->id);

    madt_t* madt = madt_get();
    madt_lapic_t* record;
    MADT_FOR_EACH(madt, record)
    {
        if (record->header.type != MADT_LAPIC)
        {
            continue;
        }

        if (record->flags & MADT_LAPIC_INITABLE && lapicId != record->lapicId)
        {
            uint8_t id = newId++;
            cpus[id] = malloc(sizeof(cpu_t));
            assert(cpus[id] != NULL);
            cpu_init(cpus[id], id, record->lapicId);
            cpuAmount++;
            assert(cpu_start(cpus[id]) != ERR);
        }
    }

    trampoline_deinit();
}

void smp_entry(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    kernel_other_init();

    printf("cpu %d: ready\n", (uint64_t)cpu->id);
    cpuReady = true;
    sched_idle_loop();
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
    smp_send_others(smp_halt_ipi);
}

void smp_ipi_receive(trap_frame_t* trapFrame, cpu_t* self)
{
    ipi_queue_t* queue = &self->queue;
    while (1)
    {
        ipi_t ipi = ipi_queue_pop(queue);
        if (ipi == NULL)
        {
            break;
        }

        ipi(trapFrame);
    }
}

void smp_send(cpu_t* cpu, ipi_t ipi)
{
    ipi_queue_t* queue = &cpu->queue;
    ipi_queue_push(queue, ipi);
    lapic_send_ipi(cpu->lapicId, VECTOR_IPI);
}

void smp_send_all(ipi_t ipi)
{
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        smp_send(cpus[id], ipi);
    }
}

void smp_send_others(ipi_t ipi)
{
    const cpu_t* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            smp_send(cpus[id], ipi);
        }
    }
}

uint8_t smp_cpu_amount(void)
{
    return cpuAmount;
}

cpu_t* smp_cpu(uint8_t id)
{
    return cpus[id];
}

cpu_t* smp_self_unsafe(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    return cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_brute(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    uint8_t lapicId = lapic_id();
    for (uint16_t id = 0; id < cpuAmount; id++)
    {
        if (cpus[id]->lapicId == lapicId)
        {
            return cpus[id];
        }
    }

    log_panic(NULL, "Unable to find cpu");
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
