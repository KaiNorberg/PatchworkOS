#include <kernel/drivers/statistics.h>

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

static dentry_t* statDir = NULL;
static dentry_t* cpuFile = NULL;
static dentry_t* memFile = NULL;

void statistics_cpu_ctx_init(statistics_cpu_ctx_t* ctx)
{
    ctx->idleClocks = 0;
    ctx->activeClocks = 0;
    ctx->interruptClocks = 0;
    ctx->interruptBegin = 0;
    ctx->interruptEnd = 0;
    lock_init(&ctx->lock);
}

static uint64_t statistics_cpu_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    char* string = malloc(MAX_PATH * (smp_cpu_amount() + 1));
    if (string == NULL)
    {
        return ERR;
    }

    strcpy(string, "cpu idle_clocks active_clocks interrupt_clocks\n");
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        cpu_t* cpu = smp_cpu(i);
        statistics_cpu_ctx_t* stat = &cpu->stat;
        LOCK_SCOPE(&stat->lock);

        clock_t now = timer_uptime();
        clock_t timeSinceLastEvent = now - stat->interruptEnd;
        if (sched_is_idle(cpu))
        {
            stat->idleClocks += timeSinceLastEvent;
        }
        else
        {
            stat->activeClocks += timeSinceLastEvent;
        }
        stat->interruptEnd = now;

        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, stat->idleClocks, stat->activeClocks,
            stat->interruptClocks, i + 1 != smp_cpu_amount() ? '\n' : '\0');
    }

    uint64_t length = strlen(string);
    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    free(string);
    return readCount;
}

static file_ops_t cpuOps = {
    .read = statistics_cpu_read,
};

static uint64_t statistics_mem_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    char* string = malloc(MAX_PATH);
    if (string == NULL)
    {
        return ERR;
    }

    sprintf(string, "value kib\ntotal %llu\nfree %llu\nreserved %llu", pmm_total_amount() * PAGE_SIZE / 1024,
        pmm_free_amount() * PAGE_SIZE / 1024, pmm_reserved_amount() * PAGE_SIZE / 1024);

    uint64_t length = strlen(string);
    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    free(string);
    return readCount;
}

static file_ops_t memOps = {
    .read = statistics_mem_read,
};

void statistics_init(void)
{
    statDir = sysfs_dir_new(NULL, "stat", NULL, NULL);
    if (statDir == NULL)
    {
        panic(NULL, "Failed to initialize statistics directory");
    }

    cpuFile = sysfs_file_new(statDir, "cpu", NULL, &cpuOps, NULL);
    if (cpuFile == NULL)
    {
        panic(NULL, "Failed to create CPU statistics file");
    }
    memFile = sysfs_file_new(statDir, "mem", NULL, &memOps, NULL);
    if (memFile == NULL)
    {
        panic(NULL, "Failed to create memory statistics file");
    }
}

void statistics_interrupt_begin(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame; // Unused

    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_SCOPE(&stat->lock);

    stat->interruptBegin = timer_uptime();

    clock_t timeBetweenTraps = stat->interruptBegin - stat->interruptEnd;
    if (sched_is_idle(self))
    {
        stat->idleClocks += timeBetweenTraps;
    }
    else
    {
        stat->activeClocks += timeBetweenTraps;
    }
}

void statistics_interrupt_end(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame; // Unused

    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_SCOPE(&stat->lock);

    stat->interruptEnd = timer_uptime();
    stat->interruptClocks += stat->interruptEnd - stat->interruptBegin;
}
