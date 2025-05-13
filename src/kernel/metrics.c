#include "metrics.h"

#include "smp.h"
#include "systime.h"

void metrics_cpu_ctx_init(metrics_cpu_ctx_t* ctx)
{
    ctx->idleClocks = 0;
    ctx->activeClocks = 0;
    ctx->trapClocks = 0;
    ctx->trapBegin = 0;
    ctx->trapEnd = 0;
}

void metrics_init(void)
{
}

void metrics_trap_begin(trap_frame_t* trapFrame, cpu_t* cpu)
{
    metrics_cpu_ctx_t* metrics = &cpu->metrics;

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

    metrics->trapEnd = systime_uptime();
    metrics->trapClocks += metrics->trapEnd - metrics->trapBegin;
}