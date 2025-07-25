#include "statistics.h"

#include "cpu/smp.h"
#include "drivers/systime/systime.h"
#include "fs/file.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"

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
    ctx->trapClocks = 0;
    ctx->trapBegin = 0;
    ctx->trapEnd = 0;
    lock_init(&ctx->lock);
}

static uint64_t statistics_cpu_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    char* string = heap_alloc(MAX_PATH * (smp_cpu_amount() + 1), HEAP_VMM);
    if (string == NULL)
    {
        return ERR;
    }

    strcpy(string, "cpu idle_clocks active_clocks trap_clocks\n");
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        cpu_t* cpu = smp_cpu(i);
        statistics_cpu_ctx_t* stat = &cpu->stat;
        LOCK_SCOPE(&stat->lock);

        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, stat->idleClocks, stat->activeClocks,
            stat->trapClocks, i + 1 != smp_cpu_amount() ? '\n' : '\0');
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
    char* string = heap_alloc(MAX_PATH, HEAP_VMM);
    if (string == NULL)
    {
        return ERR;
    }

    sprintf(string, "value kb\ntotal %llu\nfree %llu\nreserved %llu", pmm_total_amount() * PAGE_SIZE / 4,
        pmm_free_amount() * PAGE_SIZE / 4, pmm_reserved_amount() * PAGE_SIZE / 4);

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
    if (sysfs_dir_init(&statDir, sysfs_get_default(), "stat", NULL, NULL) == ERR)
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

void statistics_trap_begin(trap_frame_t* trapFrame, cpu_t* self)
{
    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_SCOPE(&stat->lock);

    stat->trapBegin = systime_uptime();

    clock_t timeBetweenTraps = stat->trapBegin - stat->trapEnd;
    if (sched_is_idle())
    {
        stat->idleClocks += timeBetweenTraps;
    }
    else
    {
        stat->activeClocks += timeBetweenTraps;
    }
}

void statistics_trap_end(trap_frame_t* trapFrame, cpu_t* self)
{
    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_SCOPE(&stat->lock);

    stat->trapEnd = systime_uptime();
    stat->trapClocks += stat->trapEnd - stat->trapBegin;
}
