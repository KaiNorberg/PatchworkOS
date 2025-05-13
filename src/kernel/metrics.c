#include "metrics.h"

#include "log.h"
#include "smp.h"
#include "sysfs.h"
#include "systime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

void metrics_cpu_ctx_init(metrics_cpu_ctx_t* ctx)
{
    ctx->idleClocks = 0;
    ctx->activeClocks = 0;
    ctx->trapClocks = 0;
    ctx->trapBegin = 0;
    ctx->trapEnd = 0;
    lock_init(&ctx->lock);
}

static uint64_t metrics_cpu_read(file_t* file, void* buffer, uint64_t count)
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
        metrics_cpu_ctx_t* metrics = &cpu->metrics;
        LOCK_DEFER(&metrics->lock);

        sprintf(&string[strlen(string)], "cpu%d %llu %llu %llu%c", cpu->id, metrics->idleClocks, metrics->activeClocks,
            metrics->trapClocks, i + 1 != smp_cpu_amount() ? '\n' : '\0');
    }

    uint64_t length = strlen(string);
    uint64_t result = BUFFER_READ(file, buffer, count, string, length);
    free(string);
    return result;
}

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(cpuOps,
    (file_ops_t){
        .read = metrics_cpu_read,
    });

void metrics_init(void)
{
    sysdir_t* dir = sysdir_new("/", "metrics", NULL, NULL);
    ASSERT_PANIC(dir != NULL);
    ASSERT_PANIC(sysdir_add(dir, "cpu", &cpuOps, NULL) != ERR);
}

void metrics_trap_begin(trap_frame_t* trapFrame, cpu_t* cpu)
{
    metrics_cpu_ctx_t* metrics = &cpu->metrics;
    LOCK_DEFER(&metrics->lock);

    metrics->trapBegin = systime_uptime();

    clock_t timeBetweenTraps = metrics->trapBegin - metrics->trapEnd;
    if (sched_thread() == NULL)
    {
        metrics->idleClocks += timeBetweenTraps;
    }
    else
    {
        metrics->activeClocks += timeBetweenTraps;
    }
}

void metrics_trap_end(trap_frame_t* trapFrame, cpu_t* cpu)
{
    metrics_cpu_ctx_t* metrics = &cpu->metrics;
    LOCK_DEFER(&metrics->lock);

    metrics->trapEnd = systime_uptime();
    metrics->trapClocks += metrics->trapEnd - metrics->trapBegin;
}