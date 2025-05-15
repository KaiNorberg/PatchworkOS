#include "statistics.h"

#include "log.h"
#include "smp.h"
#include "sysfs.h"
#include "systime.h"
#include "view.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

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

void statistics_init(void)
{
    sysdir_t* dir = sysdir_new("/", "stat", NULL, NULL);
    ASSERT_PANIC(dir != NULL);
    ASSERT_PANIC(sysdir_add(dir, "cpu", &cpuOps, NULL) != ERR);
}

void statistics_trap_begin(trap_frame_t* trapFrame, cpu_t* cpu)
{
    statistics_cpu_ctx_t* stat = &cpu->stat;
    LOCK_DEFER(&stat->lock);

    stat->trapBegin = systime_uptime();

    clock_t timeBetweenTraps = stat->trapBegin - stat->trapEnd;
    if (sched_thread() == NULL)
    {
        stat->idleClocks += timeBetweenTraps;
    }
    else
    {
        stat->activeClocks += timeBetweenTraps;
    }
}

void statistics_trap_end(trap_frame_t* trapFrame, cpu_t* cpu)
{
    statistics_cpu_ctx_t* stat = &cpu->stat;
    LOCK_DEFER(&stat->lock);

    stat->trapEnd = systime_uptime();
    stat->trapClocks += stat->trapEnd - stat->trapBegin;
}