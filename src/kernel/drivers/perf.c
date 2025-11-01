#include <kernel/drivers/perf.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/smp.h>
#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

static dentry_t* perfDir = NULL;
static dentry_t* cpuFile = NULL;
static dentry_t* memFile = NULL;

static uint64_t perf_cpu_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    char* string = malloc(MAX_PATH * (smp_cpu_amount() + 1));
    if (string == NULL)
    {
        return ERR;
    }

    strcpy(string, "cpu active_clocks interrupt_clocks\n");
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        cpu_t* cpu = smp_cpu(i);

        lock_acquire(&cpu->perf.lock);
        clock_t uptime = timer_uptime();
        clock_t activeClocks = cpu->perf.activeClocks;
        clock_t interruptClocks = cpu->perf.interruptClocks;
        lock_release(&cpu->perf.lock);

        clock_t nonIdleClocks = activeClocks + interruptClocks;
        clock_t idleClocks = uptime > nonIdleClocks ? uptime - nonIdleClocks : 0;
        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, idleClocks, activeClocks, interruptClocks,
            i + 1 != smp_cpu_amount() ? '\n' : '\0');
    }

    uint64_t length = strlen(string);
    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    free(string);
    return readCount;
}

static file_ops_t cpuOps = {
    .read = perf_cpu_read,
};

static uint64_t perf_mem_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    char* string = malloc(MAX_PATH);
    if (string == NULL)
    {
        return ERR;
    }

    int length =
        sprintf(string, "value kib\ntotal %llu\nfree %llu\nreserved %llu", pmm_total_amount() * PAGE_SIZE / 1024,
            pmm_free_amount() * PAGE_SIZE / 1024, pmm_reserved_amount() * PAGE_SIZE / 1024);
    if (length < 0)
    {
        free(string);
        return ERR;
    }

    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    free(string);
    return readCount;
}

static file_ops_t memOps = {
    .read = perf_mem_read,
};

void perf_cpu_ctx_init(perf_cpu_ctx_t* ctx)
{
    ctx->activeClocks = 0;
    ctx->interruptClocks = 0;
    ctx->lastUpdate = 0;
    ctx->lastSwitch = PERF_SWITCH_NONE;
    lock_init(&ctx->lock);
}

void perf_process_ctx_init(perf_process_ctx_t* ctx)
{
    ctx->userClocks = 0;
    ctx->kernelClocks = 0;
    ctx->startTime = timer_uptime();
    lock_init(&ctx->lock);
}

void perf_init(void)
{
    perfDir = sysfs_dir_new(NULL, "perf", NULL, NULL);
    if (perfDir == NULL)
    {
        panic(NULL, "Failed to initialize performance directory");
    }

    cpuFile = sysfs_file_new(perfDir, "cpu", NULL, &cpuOps, NULL);
    if (cpuFile == NULL)
    {
        panic(NULL, "Failed to create CPU performance file");
    }
    memFile = sysfs_file_new(perfDir, "mem", NULL, &memOps, NULL);
    if (memFile == NULL)
    {
        panic(NULL, "Failed to create memory performance file");
    }
}

void perf_update(cpu_t* self, perf_switch_t switchType)
{
    assert(self == smp_self_unsafe());

    clock_t uptime = timer_uptime();

    perf_cpu_ctx_t* cpuPerf = &self->perf;
    LOCK_SCOPE(&cpuPerf->lock);
    if (cpuPerf->lastUpdate == 0)
    {
        cpuPerf->lastUpdate = uptime;
        if (switchType != PERF_SWITCH_NONE)
        {
            cpuPerf->lastSwitch = switchType;
        }
        return;
    }

    clock_t timeSinceLastEvent = uptime - cpuPerf->lastUpdate;
    switch (cpuPerf->lastSwitch)
    {
    case PERF_SWITCH_ENTER_KERNEL_INTERRUPT:
    case PERF_SWITCH_ENTER_USER_INTERRUPT:
        cpuPerf->interruptClocks += timeSinceLastEvent;
        break;
    default:
    {
        perf_process_ctx_t* procPerf = &self->sched.runThread->process->perf;
        LOCK_SCOPE(&procPerf->lock);
        if (self->sched.runThread == self->sched.idleThread)
        {
            break;
        }

        cpuPerf->activeClocks += timeSinceLastEvent;
        switch (cpuPerf->lastSwitch)
        {
        case PERF_SWITCH_LEAVE_SYSCALL:
        case PERF_SWITCH_LEAVE_INTERRUPT:
            procPerf->userClocks += timeSinceLastEvent;
            break;
        case PERF_SWITCH_ENTER_SYSCALL:
        case PERF_SWITCH_NONE:
        default:
            procPerf->kernelClocks += timeSinceLastEvent;
            break;
        }
    }
    break;
    }

    cpuPerf->lastUpdate = uptime;
    if (switchType != PERF_SWITCH_NONE)
    {
        cpuPerf->lastSwitch = switchType;
    }
}
