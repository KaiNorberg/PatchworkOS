#include "process.h"

#include "actions.h"
#include "lock.h"
#include "log.h"
#include "rwlock.h"
#include "sched.h"
#include "sys/io.h"
#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/list.h>
#include <sys/math.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock;

static uint64_t process_cmdline_read(file_t* file, void* buffer, uint64_t count)
{
    process_t* process = file->sysobj->dir->private;
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
    return BUFFER_READ(file, buffer, count, first, length);
}

static uint64_t process_cmdline_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    process_t* process = file->sysobj->dir->private;
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

    uint64_t size = last - first;
    return BUFFER_SEEK(file, offset, origin, size);
}

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(cmdlineOps,
    (file_ops_t){
        .read = process_cmdline_read,
        .seek = process_cmdline_seek,
    });

static uint64_t process_cwd_read(file_t* file, void* buffer, uint64_t count)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    char cwd[MAX_PATH];
    lock_acquire(&process->vfsCtx.lock);
    path_to_string(&process->vfsCtx.cwd, cwd);
    lock_release(&process->vfsCtx.lock);

    uint64_t size = strlen(cwd) + 1; // Include null terminator
    return BUFFER_READ(file, buffer, count, cwd, size);
}

static uint64_t process_cwd_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    char cwd[MAX_PATH];
    lock_acquire(&process->vfsCtx.lock);
    path_to_string(&process->vfsCtx.cwd, cwd);
    lock_release(&process->vfsCtx.lock);

    uint64_t size = strlen(cwd) + 1; // Include null terminator
    return BUFFER_SEEK(file, offset, origin, size);
}

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(cwdOps,
    (file_ops_t){
        .read = process_cwd_read,
        .seek = process_cwd_seek,
    })

static uint64_t process_action_kill(uint64_t argc, const char** argv, void* private)
{
    process_t* process = private;
    atomic_store(&process->dead, true);
    return 0;
}

static uint64_t process_action_wait(uint64_t argc, const char** argv, void* private)
{
    process_t* process = private;
    WAITSYS_BLOCK(&process->queue, atomic_load(&process->dead));
    return 0;
}

static actions_t actions = {
    {"kill", process_action_kill, 1, 1},
    {"wait", process_action_wait, 1, 1},
    {0},
};

static uint64_t process_ctl_write(file_t* file, const void* buffer, uint64_t count)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    return actions_dispatch(&actions, buffer, count, process);
}

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(ctlOps,
    (file_ops_t){
        .write = process_ctl_write,
    });

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
    if (sysdir_add(dir, "ctl", &ctlOps, NULL) == ERR || sysdir_add(dir, "cwd", &cwdOps, NULL) == ERR ||
        sysdir_add(dir, "cmdline", &cmdlineOps, NULL) == ERR)
    {
        return ERR;
    }

    return 0;
}

process_t* process_new(process_t* parent, const char** argv)
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

    if (parent != NULL)
    {
        LOCK_DEFER(&parent->vfsCtx.lock);
        vfs_ctx_init(&process->vfsCtx, &parent->vfsCtx.cwd);
    }
    else
    {
        vfs_ctx_init(&process->vfsCtx, NULL);
    }

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

    list_entry_init(&process->entry);
    list_init(&process->children);
    if (parent != NULL)
    {
        RWLOCK_WRITE_DEFER(&treeLock);
        list_push(&parent->children, &process->entry);
        process->parent = parent;
    }
    else
    {
        process->parent = NULL;
    }

    return process;
}

void process_free(process_t* process)
{
    if (process->parent != NULL)
    {
        RWLOCK_WRITE_DEFER(&treeLock);
        list_remove(&process->entry);

        process_t* child;
        process_t* temp;
        LIST_FOR_EACH_SAFE(child, temp, &process->children, entry)
        {
            list_remove(&child->entry);
            child->parent = NULL;
        }
        process->parent = NULL;
    }

    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_on_free
    waitsys_unblock(&process->queue, WAITSYS_ALL);
    sysdir_free(process->dir);
}

bool process_is_child(process_t* process, pid_t parentId)
{
    RWLOCK_READ_DEFER(&treeLock);

    process_t* parent = process->parent;
    while (1)
    {
        if (parent == NULL)
        {
            return false;
        }

        if (parent->id == parentId)
        {
            return true;
        }
        parent = parent->parent;
    }
}

void process_backend_init(void)
{
    sysdir_t* selfdir = sysdir_new("/proc", "self", NULL, NULL);
    ASSERT_PANIC(selfdir != NULL);
    ASSERT_PANIC(process_dir_populate(selfdir) != ERR);

    rwlock_init(&treeLock);
}
