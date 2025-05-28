#include "process.h"

#include "fs/ctl.h"
#include "fs/vfs.h"
#include "fs/view.h"
#include "sched/sched.h"
#include "sync/lock.h"
#include "sync/rwlock.h"
#include "utils/log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock;

static process_dir_t self;

static uint64_t process_ctl_wait(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = file->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    WAIT_BLOCK(&process->queue, ({
        LOCK_DEFER(&process->threads.lock);
        process->threads.dying;
    }));
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps, PATH_NONE,
    (ctl_array_t){
        {"wait", process_ctl_wait, 1, 1},
        {0},
    });

static uint64_t process_cwd_view_init(file_t* file, view_t* view)
{
    process_t* process = file->private;
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

static uint64_t process_cmdline_view_init(file_t* file, view_t* view)
{
    process_t* process = file->private;
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

static uint64_t process_note_write(file_t* file, const void* buffer, uint64_t count)
{
    process_t* process = file->private;
    LOCK_DEFER(&process->threads.lock);

    thread_t* thread = CONTAINER_OF_SAFE(list_first(&process->threads.list), thread_t, processEntry);
    if (thread == NULL)
    {
        return ERROR(EINVAL);
    }

    if (thread_send_note(thread, buffer, count) == ERR)
    {
        return ERR;
    }

    return count;
}

SYSFS_STANDARD_OPS_DEFINE(noteOps, PATH_NONE,
    (file_ops_t){
        .write = process_note_write,
    })

static void process_dir_init(process_dir_t* dir, const char* name, process_t* process)
{
    assert(sysdir_init(&dir->sysdir, "/proc", name, process) != ERR);
    assert(sysobj_init(&dir->ctlObj, &dir->sysdir, "ctl", &ctlOps, process) != ERR);
    assert(sysobj_init(&dir->cwdObj, &dir->sysdir, "cwd", &cwdOps, process) != ERR);
    assert(sysobj_init(&dir->cmdlineObj, &dir->sysdir, "cmdline", &cmdlineOps, process) != ERR);
    assert(sysobj_init(&dir->noteObj, &dir->sysdir, "note", &noteOps, process) != ERR);
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
    wait_queue_init(&process->queue);
    futex_ctx_init(&process->futexCtx);
    process->threads.dying = false;
    process->threads.newTid = 0;
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

    char dirname[MAX_PATH];
    ulltoa(process->id, dirname, 10);
    process_dir_init(&process->dir, dirname, process);

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

static void process_on_free(sysdir_t* dir)
{
    process_t* process = dir->private;
    printf("process: on free pid=%d\n", process->id);
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

void process_free(process_t* process)
{
    assert(list_empty(&process->threads.list));

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
    sysdir_deinit(&process->dir.sysdir, process_on_free);
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
    printf("process_backend: init\n");

    process_dir_init(&self, "self", NULL);

    rwlock_init(&treeLock);
}
