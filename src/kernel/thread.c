#include "thread.h"

#include "defs.h"
#include "gdt.h"
#include "regs.h"
#include "smp.h"
#include "time.h"
#include "vfs.h"

#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

static _Atomic pid_t newPid = ATOMIC_VAR_INIT(0);

static char** process_allocate_argv(const char** src)
{
    uint64_t argc = 0;
    if (src != NULL)
    {
        while (src[argc] != NULL)
        {
            argc++;
        }
    }

    if (argc != 0)
    {
        uint64_t size = sizeof(const char*) * (argc + 1);
        for (uint64_t i = 0; i < argc; i++)
        {
            uint64_t strLen = strnlen(src[i], MAX_PATH + 1);
            if (strLen >= MAX_PATH + 1)
            {
                return NULL;
            }
            size += (strLen + 1) * sizeof(char);
        }

        char** dest = malloc(size);
        if (dest == NULL)
        {
            return NULL;
        }

        uint64_t offset = sizeof(const char*) * (argc + 1);
        for (uint64_t i = 0; i < argc; i++)
        {
            void* arg = (void*)((uint64_t)dest + offset);
            strcpy(arg, src[i]);
            dest[i] = arg;
            offset += strlen(src[i]) + 1;
        }
        dest[argc] = NULL;

        return dest;
    }
    else
    {
        char** dest = malloc(sizeof(const char*));
        if (dest == NULL)
        {
            return NULL;
        }

        dest[0] = NULL;

        return dest;
    }
}

static process_t* process_new(const char** argv)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        return ERRPTR(ENOMEM);
    }

    process->argv = process_allocate_argv(argv);
    if (process->argv == NULL)
    {
        free(process);
        return ERRPTR(ENOMEM);
    }

    process->killed = false;
    process->id = atomic_fetch_add(&newPid, 1);
    vfs_context_init(&process->vfsContext);
    space_init(&process->space);
    atomic_init(&process->ref, 0);
    atomic_init(&process->newTid, 0);

    return process;
}

static void process_free(process_t* process)
{
    vfs_context_deinit(&process->vfsContext);
    space_deinit(&process->space);
    free(process->argv);
    free(process);
}

static thread_t* process_thread_new(process_t* process, void* entry, priority_t priority)
{
    atomic_fetch_add(&process->ref, 1);

    thread_t* thread = malloc(sizeof(thread_t));
    if (thread == NULL)
    {
        atomic_fetch_sub(&process->ref, 1);
        return NULL;
    }
    list_entry_init(&thread->entry);
    thread->process = process;
    thread->id = atomic_fetch_add(&thread->process->newTid, 1);
    thread->killed = false;
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->block.deadline = 0;
    thread->block.result = BLOCK_NORM;
    thread->block.blocker = NULL;
    thread->error = 0;
    thread->priority = MIN(priority, PRIORITY_MAX);
    if (simd_context_init(&thread->simdContext) == ERR)
    {
        atomic_fetch_sub(&process->ref, 1);
        free(thread);
        return NULL;
    }
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = ((uint64_t)thread->kernelStack) + CONFIG_KERNEL_STACK;
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    return thread;
}

thread_t* thread_new(const char** argv, void* entry, priority_t priority)
{
    process_t* process = process_new(argv);
    if (process == NULL)
    {
        return ERRPTR(EINVAL);
    }

    thread_t* thread = process_thread_new(process, entry, priority);
    if (thread == NULL)
    {
        process_free(process);
        return ERRPTR(EINVAL);
    }

    return thread;
}

void thread_free(thread_t* thread)
{
    if (atomic_fetch_sub(&thread->process->ref, 1) <= 1)
    {
        process_free(thread->process);
    }

    simd_context_deinit(&thread->simdContext);
    free(thread);
}

thread_t* thread_split(thread_t* thread, void* entry, priority_t priority)
{
    return process_thread_new(thread->process, entry, priority);
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
        trapFrame->rsp = (uint64_t)self->idleStack + CPU_IDLE_STACK_SIZE;

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
