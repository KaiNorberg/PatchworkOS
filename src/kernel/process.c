#include "process.h"

#include "sched.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

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
    if (first == NULL)
    {
        return 0;
    }
    char* last = (char*)((uint64_t)process->argv.buffer + process->argv.size);
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

static file_ops_t cwdFileOps = {
    .read = process_cwd_read,
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
    printf("process: on_free pid=%d", process->id);
    // vfs_ctx_deinit() is in process_free
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static uint64_t process_dir_populate(sysdir_t* dir)
{
    if (sysdir_add(dir, "ctl", &ctlResOps, NULL) == ERR || sysdir_add(dir, "cwd", &cwdResOps, NULL) == ERR ||
        sysdir_add(dir, "cmdline", &cmdlineResOps, NULL) == ERR)
    {
        return ERR;
    }

    return 0;
}

process_t* process_new(const char** argv, const path_t* cwd)
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
    process->dir = sysdir_new("/proc", dirname, process_on_free, process);
    if (process->dir == NULL)
    {
        return NULL;
    }
    atomic_init(&process->dead, false);
    vfs_ctx_init(&process->vfsCtx, cwd);
    space_init(&process->space);
    atomic_init(&process->threadCount, 0);
    wait_queue_init(&process->queue);
    futex_ctx_init(&process->futexCtx);
    atomic_init(&process->newTid, 0);

    if (process_dir_populate(process->dir) == ERR)
    {
        process_free(process);
        return NULL;
    }

    return process;
}

void process_free(process_t* process)
{
    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_on_free
    waitsys_unblock(&process->queue, WAITSYS_ALL);
    sysdir_free(process->dir);
}

void process_self_expose(void)
{
    sysdir_t* selfdir = sysdir_new("/proc", "self", NULL, NULL);
    ASSERT_PANIC(selfdir != NULL);
    ASSERT_PANIC(process_dir_populate(selfdir) != ERR);
}
