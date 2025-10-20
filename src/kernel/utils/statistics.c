#include "statistics.h"

#include "cpu/cpu.h"
#include "cpu/smp.h"
#include "fs/file.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sync/lock.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/math.h>

static sysfs_dir_t statDir;
static sysfs_file_t cpuFile;
static sysfs_file_t memFile;

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

    char* string = heap_alloc(MAX_PATH * (smp_cpu_amount() + 1), HEAP_VMM);
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

        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, stat->idleClocks, stat->activeClocks,
            stat->interruptClocks, i + 1 != smp_cpu_amount() ? '\n' : '\0');
    }

    uint64_t length = strlen(string);
    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    heap_free(string);
    return readCount;
}

static file_ops_t cpuOps = {
    .read = statistics_cpu_read,
};

static uint64_t statistics_mem_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    char* string = heap_alloc(MAX_PATH, HEAP_VMM);
    if (string == NULL)
    {
        return ERR;
    }

    sprintf(string, "value kb\ntotal %llu\nfree %llu\nreserved %llu", pmm_total_amount() * PAGE_SIZE / 1024,
        pmm_free_amount() * PAGE_SIZE / 1024, pmm_reserved_amount() * PAGE_SIZE / 1024);

    uint64_t length = strlen(string);
    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, length);
    heap_free(string);
    return readCount;
}

static file_ops_t memOps = {
    .read = statistics_mem_read,
};

void statistics_init(void)
{
    if (sysfs_dir_init(&statDir, sysfs_get_dev(), "stat", NULL, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize statistics directory");
    }
    if (sysfs_file_init(&cpuFile, &statDir, "cpu", NULL, &cpuOps, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize CPU statistics file");
    }
    if (sysfs_file_init(&memFile, &statDir, "mem", NULL, &memOps, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize memory statistics file");
    }
}

void statistics_interrupt_begin(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame; // Unused

    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_SCOPE(&stat->lock);

    stat->interruptBegin = timer_uptime();

    clock_t timeBetweenTraps = stat->interruptBegin - stat->interruptEnd;
    if (sched_is_idle())
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
