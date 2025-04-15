#include "thread.h"

#include "defs.h"
#include "futex.h"
#include "gdt.h"
#include "regs.h"
#include "smp.h"
#include "sysfs.h"
#include "systime.h"
#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

static uint64_t argv_init(argv_t* argv, const char** src)
{
    if (src == NULL)
    {
        argv->buffer = malloc(sizeof(const char*));
        argv->size = sizeof(const char*);
        argv->amount = 1;

        argv->buffer[0] = NULL;
        return 0;
    }

    uint64_t argc = 0;
    while (src[argc] != NULL)
    {
        argc++;
    }

    uint64_t size = sizeof(const char*) * (argc + 1);
    for (uint64_t i = 0; i < argc; i++)
    {
        uint64_t strLen = strnlen(src[i], MAX_PATH + 1);
        if (strLen >= MAX_PATH + 1)
        {
            return ERR;
        }
        size += strLen + 1;
    }

    char** dest = malloc(size);
    if (dest == NULL)
    {
        return ERR;
    }

    char* strings = (char*)((uintptr_t)dest + sizeof(char*) * (argc + 1));
    for (uint64_t i = 0; i < argc; i++)
    {
        dest[i] = strings;
        strcpy(strings, src[i]);
        strings += strlen(src[i]) + 1;
    }
    dest[argc] = NULL;

    argv->buffer = dest;
    argv->size = size;
    argv->amount = argc;

    return 0;
}

static void argv_deinit(argv_t* argv)
{
    free(argv->buffer);
}

static uint64_t process_cmdline_read(file_t* file, void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }

    process_t* process = file->resource->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    char* first = process->argv.buffer[0];
    char* last = first + process->argv.size;
    uint64_t length = last - first;

    uint64_t readCount = MIN(count, length - file->pos);
    if (readCount == 0)
    {
        return 0;
    }

    memcpy(buffer, first + file->pos, readCount);

    file->pos += readCount;
    return readCount;
}

static file_ops_t cmdlineFileOps = {
    .read = process_cmdline_read,
};

SYSFS_STANDARD_RESOURCE_OPS(cmdlineResOps, &cmdlineFileOps)

static uint64_t process_cwd_read(file_t* file, void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }

    process_t* process = file->resource->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    char cwd[MAX_PATH];
    lock_acquire(&process->vfsCtx.lock);
    path_to_string(&process->vfsCtx.cwd, cwd);
    lock_release(&process->vfsCtx.lock);

    uint64_t cwdLen = strlen(cwd) + 1; // Include \0 char.
    uint64_t readCount = MIN(count, cwdLen - file->pos);
    if (readCount == 0)
    {
        return 0;
    }

    memcpy(buffer, cwd + file->pos, readCount);

    file->pos += readCount;
    return readCount;
}

static uint64_t process_cwd_write(file_t* file, const void* buffer, uint64_t count)
{
    process_t* process = file->resource->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    return ERROR(EIMPL);
}

static file_ops_t cwdFileOps = {
    .read = process_cwd_read,
    .write = process_cwd_write,
};

SYSFS_STANDARD_RESOURCE_OPS(cwdResOps, &cwdFileOps)

/*static uint64_t process_kill_action(file_t* file, const char** argv, uint64_t argc)
{
    process_t* process = file->resource->private;
    atomic_store(&process->dead, true);
}

static uint64_t process_wait_action(file_t* file, const char** argv, uint64_t argc)
{
    process_t* process = file->resource->private;
    WAITSYS_BLOCK(&process->queue, atomic_load(&process->dead));
}

static action_table_t actions = {
    {"kill", process_kill_action, 0, 0, "Kills the process immediately"},
    {"wait", process_wait_action, 0, 0, "Blocks until the process is killed"},
    {0},
};*/

// ACTION_STANDARD_RESOURCE_WRITE(process_write, &actions);
//
static uint64_t process_ctl_write(file_t* file, const void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }

    process_t* process = file->resource->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    // TODO: Implement a proper command system with args, actions?
    if (strncmp(buffer, "kill", count) == 0)
    {
        atomic_store(&process->dead, true);
        return 4;
    }
    else if (strncmp(buffer, "wait", count) == 0)
    {
        WAITSYS_BLOCK(&process->queue, atomic_load(&process->dead));
        return 4;
    }
    else
    {
        return ERROR(EREQ);
    }
}

static file_ops_t ctlFileOps = {
    .write = process_ctl_write,
};

SYSFS_STANDARD_RESOURCE_OPS(ctlResOps, &ctlFileOps)

static void process_on_free(sysdir_t* dir)
{
    process_t* process = dir->private;
    // vfs_ctx_deinit() is in process_free
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static sysdir_t* process_dir_create(const char* name, void* private)
{
    sysdir_t* dir = sysfs_mkdir("/proc", name, process_on_free, private);
    if (dir == NULL)
    {
        return NULL;
    }

    if (sysfs_create(dir, "ctl", &ctlResOps, NULL) == ERR || sysfs_create(dir, "cwd", &cwdResOps, NULL) == ERR || sysfs_create(dir, "cmdline", &cmdlineResOps, NULL) == ERR)
    {
        sysfs_rmdir(dir);
        return NULL;
    }

    return dir;
}

static process_t* process_new(const char** argv, const path_t* cwd)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        return ERRPTR(ENOMEM);
    }

    process->id = atomic_fetch_add(&newPid, 1);
    if (argv_init(&process->argv, argv) == ERR)
    {
        free(process);
        return ERRPTR(ENOMEM);
    }

    char dirname[MAX_PATH];
    ulltoa(process->id, dirname, 10);
    process->dir = process_dir_create(dirname, process);
    if (process->dir == NULL)
    {
        argv_deinit(&process->argv);
        free(process);
        return NULL;
    }
    atomic_init(&process->dead, false);
    vfs_ctx_init(&process->vfsCtx, cwd);
    space_init(&process->space);
    atomic_init(&process->threadCount, 0);
    wait_queue_init(&process->queue);
    futex_ctx_init(&process->futexCtx);
    atomic_init(&process->newTid, 0);

    return process;
}

static void process_free(process_t* process)
{
    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_on_free
    waitsys_unblock(&process->queue, WAITSYS_ALL);
    sysfs_rmdir(process->dir);
}

static thread_t* process_thread_create(process_t* process, void* entry, priority_t priority)
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
    thread->dead = false;
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->block.waitEntries[0] = NULL;
    thread->block.entryAmount = 0;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = 0;
    thread->error = 0;
    thread->priority = MIN(priority, PRIORITY_MAX);
    if (simd_ctx_init(&thread->simdCtx) == ERR)
    {
        atomic_fetch_sub(&process->threadCount, 1);
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

void process_self_init(void)
{
    process_dir_create("self", NULL);
}

thread_t* thread_new(const char** argv, void* entry, priority_t priority, const path_t* cwd)
{
    process_t* process = process_new(argv, cwd);
    if (process == NULL)
    {
        return NULL;
    }

    thread_t* thread = process_thread_create(process, entry, priority);
    if (thread == NULL)
    {
        process_free(process);
        return ERRPTR(ENOMEM);
    }

    return thread;
}

thread_t* thread_new_inherit(thread_t* thread, void* entry, priority_t priority)
{
    return process_thread_create(thread->process, entry, priority);
}

void thread_free(thread_t* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        process_free(thread->process);
    }

    simd_ctx_deinit(&thread->simdCtx);
    free(thread);
}

void thread_save(thread_t* thread, const trap_frame_t* trapFrame)
{
    simd_ctx_save(&thread->simdCtx);
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
        simd_ctx_load(&thread->simdCtx);
    }
}
