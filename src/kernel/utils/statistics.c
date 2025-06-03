#include "statistics.h"

#include "cpu/smp.h"
#include "drivers/systime/systime.h"
#include "fs/sysfs.h"
#include "fs/view.h"
#include "log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

static sysdir_t statDir;
static sysobj_t cpuObj;
static sysobj_t memObj;

void statistics_cpu_ctx_init(statistics_cpu_ctx_t* ctx)
{
    ctx->idleClocks = 0;
    ctx->activeClocks = 0;
    ctx->trapClocks = 0;
    ctx->trapBegin = 0;
    ctx->trapEnd = 0;
    lock_init(&ctx->lock);
}

static uint64_t statistics_cpu_view_init(file_t* file, view_t* view)
{
    char* string = malloc(MAX_PATH * (smp_cpu_amount() + 1));
    if (string == NULL)
    {
        return ERR;
    }

    strcpy(string, "cpu idle_clocks active_clocks trap_clocks\n");
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        cpu_t* cpu = smp_cpu(i);
        statistics_cpu_ctx_t* stat = &cpu->stat;
        LOCK_DEFER(&stat->lock);

        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, stat->idleClocks, stat->activeClocks,
            stat->trapClocks, i + 1 != smp_cpu_amount() ? '\n' : '\0');
    }

    view->buffer = string;
    view->length = strlen(string) + 1;
    return 0;
}

static void statistics_cpu_view_deinit(view_t* view)
{
    free(view->buffer);
}

VIEW_STANDARD_OPS_DEFINE(cpuOps, PATH_NONE,
    (view_ops_t){
        .init = statistics_cpu_view_init,
        .deinit = statistics_cpu_view_deinit,
    });

static uint64_t statistics_mem_view_init(file_t* file, view_t* view)
{
    char* string = malloc(MAX_PATH);
    if (string == NULL)
    {
        return ERR;
    }

    sprintf(string, "value kb\ntotal %d\nfree %d\nreserved %d", pmm_total_amount() * PAGE_SIZE / 4,
        pmm_free_amount() * PAGE_SIZE / 4, pmm_reserved_amount() * PAGE_SIZE / 4);

    view->buffer = string;
    view->length = strlen(string) + 1;
    return 0;
}

static void statistics_mem_view_deinit(view_t* view)
{
    free(view->buffer);
}

VIEW_STANDARD_OPS_DEFINE(memOps, PATH_NONE,
    (view_ops_t){
        .init = statistics_mem_view_init,
        .deinit = statistics_mem_view_deinit,
    });

void statistics_init(void)
{
    assert(sysdir_init(&statDir, "/", "stat", NULL) != ERR);
    assert(sysobj_init(&cpuObj, &statDir, "cpu", &cpuOps, NULL) != ERR);
    assert(sysobj_init(&memObj, &statDir, "mem", &memOps, NULL) != ERR);
}

void statistics_trap_begin(trap_frame_t* trapFrame, cpu_t* self)
{
    statistics_cpu_ctx_t* stat = &self->stat;
    LOCK_DEFER(&stat->lock);

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
    LOCK_DEFER(&stat->lock);

    stat->trapEnd = systime_uptime();
    stat->trapClocks += stat->trapEnd - stat->trapBegin;
}