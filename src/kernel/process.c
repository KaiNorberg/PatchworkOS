#include "process.h"

#include "ctl.h"
#include "lock.h"
#include "log.h"
#include "rwlock.h"
#include "sched.h"
#include "vfs.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock;

static uint64_t process_cmdline_view_init(file_t* file, view_t* view)
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

    view->buffer = first;
    view->length = length;
    return 0;
}

VIEW_STANDARD_OPS_DEFINE(cmdlineOps, PATH_NONE,
    (view_ops_t){
        .init = process_cmdline_view_init,
    });

static uint64_t process_cwd_view_init(file_t* file, view_t* view)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    char* cwd = malloc(MAX_PATH);
    if (cwd == NULL)
    {
        return ERR;
    }

    lock_acquire(&process->vfsCtx.lock);
    path_to_string(&process->vfsCtx.cwd, cwd);
    lock_release(&process->vfsCtx.lock);

    view->length = strlen(cwd) + 1;
    view->buffer = cwd;
    return 0;
}

static void process_cwd_view_deinit(view_t* view)
{
    free(view->buffer);
}

VIEW_STANDARD_OPS_DEFINE(cwdOps, PATH_NONE,
    (view_ops_t){
        .init = process_cwd_view_init,
        .deinit = process_cwd_view_deinit,
    });

static uint64_t process_ctl_kill(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    process_kill(process);
    return 0;
}

static uint64_t process_ctl_wait(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = file->sysobj->dir->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    WAIT_BLOCK(&process->queue, atomic_load(&process->dead));
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps, PATH_NONE,
    (ctl_array_t){
        {"kill", process_ctl_kill, 1, 1},
        {"wait", process_ctl_wait, 1, 1},
        {0},
    });

static uint64_t process_dir_populate(sysdir_t* dir)
{
    if (sysdir_add(dir, "ctl", &ctlOps, NULL) == ERR || sysdir_add(dir, "cwd", &cwdOps, NULL) == ERR ||
        sysdir_add(dir, "cmdline", &cmdlineOps, NULL) == ERR)
    {
        return ERR;
    }

    return 0;
}

static void process_on_free(sysdir_t* dir)
{
    process_t* process = dir->private;
    printf("process: on_free pid=%d\n", process->id);
    // vfs_ctx_deinit() is in process_free
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
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
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

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
    wait_unblock(&process->queue, WAIT_ALL);
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

void process_kill(process_t* process)
{
    LOCK_DEFER(&process->threads.lock);

    thread_t* thread;
    LIST_FOR_EACH(thread, &process->threads.list, processEntry)
    {
        thread_state_t state = atomic_exchange(&thread->state, THREAD_DEAD);
        if (state == THREAD_BLOCKED)
        {
            wait_unblock_thread(thread, WAIT_DEAD, NULL, true);
        }
    }

    atomic_store(&process->dead, true);
}

void process_backend_init(void)
{
    printf("process_backend: init\n");
    sysdir_t* selfdir = sysdir_new("/proc", "self", NULL, NULL);
    ASSERT_PANIC(selfdir != NULL);
    ASSERT_PANIC(process_dir_populate(selfdir) != ERR);

    rwlock_init(&treeLock);
}
