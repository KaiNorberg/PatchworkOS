#include "process.h"

#include "debug.h"
#include "gdt.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "smp.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

process_t* process_new(const char* executable)
{
    process_t* process = malloc(sizeof(process_t));
    process->killed = false;
    process->id = atomic_fetch_add(&newPid, 1);
    memset(process->executable, 0, MAX_PATH);
    if (executable != NULL && vfs_realpath(process->executable, executable) == ERR)
    {
        memset(process->executable, 0, MAX_PATH);
    }
    vfs_context_init(&process->vfsContext);
    space_init(&process->space);
    atomic_init(&process->threadCount, 0);
    atomic_init(&process->newTid, 0);

    return process;
}

thread_t* thread_new(process_t* process, void* entry, uint8_t priority)
{
    atomic_fetch_add(&process->threadCount, 1);

    thread_t* thread = malloc(sizeof(thread_t));
    list_entry_init(&thread->base);
    thread->process = process;
    thread->id = atomic_fetch_add(&process->newTid, 1);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->error = 0;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = priority;
    simd_context_init(&thread->simdContext);
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = ((uint64_t)thread->kernelStack) + CONFIG_KERNEL_STACK;
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    return thread;
}

void thread_free(thread_t* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        vfs_context_cleanup(&thread->process->vfsContext);
        space_cleanup(&thread->process->space);
        free(thread->process);
    }

    simd_context_cleanup(&thread->simdContext);
    free(thread);
}

void thread_save(thread_t* thread, const trap_frame_t* trapFrame)
{
    simd_context_save(&thread->simdContext);
    thread->trapFrame = *trapFrame;
}

void thread_load(thread_t* thread, trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();

    if (thread == NULL)
    {
        memset(trapFrame, 0, sizeof(trap_frame_t));
        trapFrame->rip = (uint64_t)sched_idle_loop;
        trapFrame->cs = GDT_KERNEL_CODE;
        trapFrame->ss = GDT_KERNEL_DATA;
        trapFrame->rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
        trapFrame->rsp = (uint64_t)smp_self_unsafe()->idleStack + CPU_IDLE_STACK_SIZE;

        space_load(NULL);
        tss_stack_load(&self->tss, NULL);
    }
    else
    {
        thread->timeStart = time_uptime();
        thread->timeEnd = thread->timeStart + CONFIG_TIME_SLICE;

        *trapFrame = thread->trapFrame;

        space_load(&thread->process->space);
        tss_stack_load(&self->tss, (void*)((uint64_t)thread->kernelStack + CONFIG_KERNEL_STACK));
        simd_context_load(&thread->simdContext);
    }
}
