#include <kernel/cpu/interrupt.h>
#include <kernel/drivers/perf.h>

#include <kernel/cpu/cpu.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/utils.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/status.h>

static dentry_t* perfDir = NULL;
static dentry_t* cpuFile = NULL;
static dentry_t* memFile = NULL;

typedef struct
{
    clock_t activeClocks;
    clock_t interruptClocks;
    clock_t idleClocks;
    clock_t interruptBegin;
    clock_t interruptEnd;
    lock_t lock;
} perf_cpu_t;

PERCPU_DEFINE_CTOR(static perf_cpu_t, pcpu_perf)
{
    perf_cpu_t* perf = SELF_PTR(pcpu_perf);

    perf->activeClocks = 0;
    perf->interruptClocks = 0;
    perf->idleClocks = 0;
    perf->interruptBegin = 0;
    perf->interruptEnd = 0;
    lock_init(&perf->lock);
}

static status_t perf_cpu_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    UNUSED(file);

    char* string = malloc(256 * (cpu_amount() + 1));
    if (string == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    strcpy(string, "cpu idle_clocks active_clocks interrupt_clocks");

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        sprintf(string + strlen(string), "\n");

        perf_cpu_t* perf = CPU_PTR(cpu->id, pcpu_perf);

        lock_acquire(&perf->lock);
        clock_t uptime = clock_uptime();
        clock_t delta = uptime - perf->interruptEnd;
        if (sched_is_idle(cpu))
        {
            perf->idleClocks += delta;
        }
        else
        {
            perf->activeClocks += delta;
        }
        perf->interruptEnd = uptime;

        clock_t volatile activeClocks = perf->activeClocks;
        clock_t volatile interruptClocks = perf->interruptClocks;
        clock_t volatile idleClocks = perf->idleClocks;
        lock_release(&perf->lock);

        int length =
            sprintf(string + strlen(string), "%lu %lu %lu %lu", cpu->id, idleClocks, activeClocks, interruptClocks);
        if (length < 0)
        {
            free(string);
            return ERR(DRIVER, IMPL);
        }
    }

    size_t length = strlen(string);
    status_t status = buffer_read(buffer, count, offset, bytesRead, string, length);
    free(string);
    return status;
}

static file_ops_t cpuOps = {
    .read = perf_cpu_read,
};

static status_t perf_mem_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    UNUSED(file);

    char* string = malloc(256);
    if (string == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }

    int length = sprintf(string, "total_pages %lu\nfree_pages %lu\nused_pages %lu", pmm_total_pages(),
        pmm_avail_pages(), pmm_used_pages());
    if (length < 0)
    {
        free(string);
        return ERR(DRIVER, IMPL);
    }

    status_t status = buffer_read(buffer, count, offset, bytesRead, string, length);
    free(string);
    return status;
}

static file_ops_t memOps = {
    .read = perf_mem_read,
};

void perf_process_ctx_init(perf_process_ctx_t* ctx)
{
    atomic_init(&ctx->userClocks, 0);
    atomic_init(&ctx->kernelClocks, 0);
    ctx->startTime = clock_uptime();
}

void perf_thread_ctx_init(perf_thread_ctx_t* ctx)
{
    ctx->syscallBegin = 0;
    ctx->syscallEnd = 0;
}

void perf_init(void)
{
    perfDir = devfs_dir_new(NULL, "perf", NULL, NULL);
    if (perfDir == NULL)
    {
        panic(NULL, "Failed to initialize performance directory");
    }

    cpuFile = devfs_file_new(perfDir, "cpu", NULL, &cpuOps, NULL);
    if (cpuFile == NULL)
    {
        panic(NULL, "Failed to create CPU performance file");
    }
    memFile = devfs_file_new(perfDir, "mem", NULL, &memOps, NULL);
    if (memFile == NULL)
    {
        panic(NULL, "Failed to create memory performance file");
    }
}

void perf_interrupt_begin(void)
{
    perf_cpu_t* perf = SELF_PTR(pcpu_perf);
    LOCK_SCOPE(&perf->lock);

    if (perf->interruptEnd < perf->interruptBegin)
    {
        LOG_WARN("unexpected call to perf_interrupt_begin()\n");
        return;
    }

    perf->interruptBegin = clock_uptime();
    clock_t delta = perf->interruptBegin - perf->interruptEnd;
    if (sched_is_idle(SELF->self))
    {
        perf->idleClocks += delta;
    }
    else
    {
        perf->activeClocks += delta;
    }

    thread_t* thread = thread_current_unsafe();
    // Do not count interrupt time as part of syscalls
    if (thread->perf.syscallEnd < thread->perf.syscallBegin)
    {
        clock_t syscallDelta = perf->interruptBegin - thread->perf.syscallBegin;
        atomic_fetch_add(&thread->process->perf.kernelClocks, syscallDelta);
    }
    else
    {
        clock_t userDelta = perf->interruptBegin - thread->perf.syscallEnd;
        atomic_fetch_add(&thread->process->perf.userClocks, userDelta);
    }
}

void perf_interrupt_end(void)
{
    perf_cpu_t* perf = SELF_PTR(pcpu_perf);

    LOCK_SCOPE(&perf->lock);

    perf->interruptEnd = clock_uptime();
    perf->interruptClocks += perf->interruptEnd - perf->interruptBegin;

    thread_t* thread = thread_current_unsafe();
    if (thread->perf.syscallEnd < thread->perf.syscallBegin)
    {
        thread->perf.syscallBegin = perf->interruptEnd;
    }
    else
    {
        thread->perf.syscallEnd = perf->interruptEnd;
    }
}

void perf_syscall_begin(void)
{
    CLI_SCOPE();

    thread_t* thread = thread_current_unsafe();
    perf_thread_ctx_t* perf = &thread->perf;

    clock_t uptime = clock_uptime();
    if (perf->syscallEnd < perf->syscallBegin)
    {
        LOG_WARN("unexpected call to perf_syscall_begin()\n");
        return;
    }

    if (perf->syscallEnd != 0)
    {
        atomic_fetch_add(&thread->process->perf.userClocks, uptime - perf->syscallEnd);
    }

    perf->syscallBegin = uptime;
}

void perf_syscall_end(void)
{
    CLI_SCOPE();

    thread_t* thread = thread_current_unsafe();
    perf_thread_ctx_t* perf = &thread->perf;
    process_t* process = thread->process;

    perf->syscallEnd = clock_uptime();
    clock_t delta = perf->syscallEnd - perf->syscallBegin;

    atomic_fetch_add(&process->perf.kernelClocks, delta);
}
