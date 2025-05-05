#include "thread.h"

#include "defs.h"
#include "futex.h"
#include "gdt.h"
#include "lock.h"
#include "log.h"
#include "path.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "sys/io.h"
#include "sysfs.h"
#include "systime.h"
#include "vfs.h"
#include "waitsys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

thread_t* thread_new(process_t* process, void* entry, priority_t priority)
{
    atomic_fetch_add(&process->threadCount, 1);

    thread_t* thread = malloc(sizeof(thread_t));
    if (thread == NULL)
    {
        atomic_fetch_sub(&process->threadCount, 1);
        return NULL;
    }
    list_entry_init(&thread->entry);
    thread->process = process;
    thread->id = atomic_fetch_add(&thread->process->newTid, 1);
    atomic_init(&thread->dead, false);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    waitsys_thread_ctx_init(&thread->waitsys);
    thread->error = 0;
    thread->priority = MIN(priority, PRIORITY_MAX);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        atomic_fetch_sub(&process->threadCount, 1);
        free(thread);
        return NULL;
    }
    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = ((uint64_t)thread->kernelStack) + CONFIG_KERNEL_STACK;
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

    return thread;
}

void thread_free(thread_t* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        process_free(thread->process);
    }

    simd_ctx_deinit(&thread->simd);
    free(thread);
}

void thread_save(thread_t* thread, const trap_frame_t* trapFrame)
{
    simd_ctx_save(&thread->simd);
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
        trapFrame->rsp = (uint64_t)self->idleStack + CPU_IDLE_STACK_SIZE;

        space_load(NULL);
        tss_stack_load(&self->tss, NULL);
    }
    else
    {
        thread->timeStart = systime_uptime();
        thread->timeEnd = thread->timeStart + CONFIG_TIME_SLICE;

        *trapFrame = thread->trapFrame;

        space_load(&thread->process->space);
        tss_stack_load(&self->tss, (void*)((uint64_t)thread->kernelStack + CONFIG_KERNEL_STACK));
        simd_ctx_load(&thread->simd);
    }
}

bool thread_dead(thread_t* thread)
{
    return atomic_load(&thread->dead) || atomic_load(&thread->process->dead);
}
