#include <kernel/cpu/interrupt.h>
#include <kernel/drivers/perf.h>

#include <kernel/cpu/cpu.h>
#include <kernel/fs/file.h>
#include <kernel/fs/sysfs.h>
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
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>

static dentry_t* perfDir = NULL;
static dentry_t* cpuFile = NULL;
static dentry_t* memFile = NULL;

static uint64_t perf_cpu_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    UNUSED(file);

    char* string = malloc(256 * (cpu_amount() + 1));
    if (string == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    strcpy(string, "cpu idle_clocks active_clocks interrupt_clocks");

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        sprintf(string + strlen(string), "\n");

        lock_acquire(&cpu->perf.lock);
        clock_t uptime = clock_uptime();
        clock_t delta = uptime - cpu->perf.interruptEnd;
        if (sched_is_idle(cpu))
        {
            cpu->perf.idleClocks += delta;
        }
        else
        {
            cpu->perf.activeClocks += delta;
        }
        cpu->perf.interruptEnd = uptime;

        clock_t volatile activeClocks = cpu->perf.activeClocks;
        clock_t volatile interruptClocks = cpu->perf.interruptClocks;
        clock_t volatile idleClocks = cpu->perf.idleClocks;
        lock_release(&cpu->perf.lock);

        int length =
            sprintf(string + strlen(string), "%lu %lu %lu %lu", cpu->id, idleClocks, activeClocks, interruptClocks);
        if (length < 0)
        {
            free(string);
            errno = EIO;
            return ERR;
        }
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
    UNUSED(file);

    char* string = malloc(256);
    if (string == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    int length = sprintf(string, "total_pages %lu\nfree_pages %lu\nused_pages %lu", pmm_total_amount(),
        pmm_free_amount(), pmm_used_amount());
    if (length < 0)
    {
        free(string);
        errno = EIO;
        return ERR;
    }

    uint64_t readCount = BUFFER_READ(buffer, count, offset, string, (uint64_t)length);
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
    ctx->idleClocks = 0;
    ctx->interruptBegin = 0;
    ctx->interruptEnd = 0;
    lock_init(&ctx->lock);
}

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

void perf_interrupt_begin(cpu_t* self)
{
    perf_cpu_ctx_t* perf = &self->perf;
    LOCK_SCOPE(&perf->lock);

    if (perf->interruptEnd < perf->interruptBegin)
    {
        LOG_WARN("unexpected call to perf_interrupt_begin()\n");
        return;
    }

    perf->interruptBegin = clock_uptime();
    clock_t delta = perf->interruptBegin - perf->interruptEnd;
    if (sched_is_idle(self))
    {
        perf->idleClocks += delta;
    }
    else
    {
        perf->activeClocks += delta;
    }

    thread_t* thread = sched_thread_unsafe();
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

void perf_interrupt_end(cpu_t* self)
{
    LOCK_SCOPE(&self->perf.lock);

    self->perf.interruptEnd = clock_uptime();
    self->perf.interruptClocks += self->perf.interruptEnd - self->perf.interruptBegin;

    thread_t* thread = sched_thread_unsafe();
    if (thread->perf.syscallEnd < thread->perf.syscallBegin)
    {
        thread->perf.syscallBegin = self->perf.interruptEnd;
    }
    else
    {
        thread->perf.syscallEnd = self->perf.interruptEnd;
    }
}

void perf_syscall_begin(void)
{
    interrupt_disable();

    thread_t* thread = sched_thread_unsafe();
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

    interrupt_enable();
}

void perf_syscall_end(void)
{
    interrupt_disable();

    thread_t* thread = sched_thread_unsafe();
    perf_thread_ctx_t* perf = &thread->perf;
    process_t* process = thread->process;

    perf->syscallEnd = clock_uptime();
    clock_t delta = perf->syscallEnd - perf->syscallBegin;

    atomic_fetch_add(&process->perf.kernelClocks, delta);

    interrupt_enable();
}
