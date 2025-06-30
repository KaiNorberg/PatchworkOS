#include "process.h"

#include "fs/ctl.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sync/rwlock.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>

// TODO: Reimplement without view_t.

static process_t* kernelProcess = NULL;

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock;

static process_dir_t self;

static process_t* process_file_get_process(file_t* file)
{
    process_t* process = file->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    if (process == kernelProcess)
    {
        return ERRPTR(EACCES);
    }

    return process;
}

static uint64_t process_ctl_wait(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    WAIT_BLOCK(&process->queue, ({
        LOCK_DEFER(&process->threads.lock);
        process->threads.isDying;
    }));
    return 0;
}

static uint64_t process_ctl_prio(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    int prio = atoi(argv[1]);
    if (prio < 0)
    {
        return ERROR(EINVAL);
    }
    if (prio >= PRIORITY_MAX_USER)
    {
        return ERROR(EACCES);
    }

    atomic_store(&process->priority, prio);
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps, PATH_NONE,
    (ctl_array_t){
        {"wait", process_ctl_wait, 1, 1},
        {"prio", process_ctl_prio, 2, 2},
        {0},
    });

/*static uint64_t process_cwd_view_init(file_t* file, view_t* view)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    char* cwd = heap_alloc(MAX_PATH, HEAP_NONE);
    if (cwd == NULL)
    {
        return ERR;
    }
    cwd[0] = '\0';

    lock_acquire(&process->vfsCtx.lock);
    //path_to_string(&process->vfsCtx.cwd, cwd); // TODO: Implement path to string conversion.
    lock_release(&process->vfsCtx.lock);

    view->length = strlen(cwd) + 1;
    view->buffer = cwd;
    return 0;
}

static void process_cwd_view_deinit(view_t* view)
{
    heap_free(view->buffer);
}*/

static file_ops_t cwdOps =
{

};

/*static uint64_t process_cmdline_view_init(file_t* file, view_t* view)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
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
}*/

static file_ops_t cmdlineOps =
{

};

static uint64_t process_note_write(file_t* file, const void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

static file_ops_t noteOps =
{
    .write = process_note_write,
};

static void process_dir_init(process_dir_t* dir, const char* name, process_t* process)
{
    assert(sysdir_init(&dir->sysdir, "/proc", name, process) != ERR);
    assert(sysobj_init(&dir->ctlObj, &dir->sysdir, "ctl", &ctlOps, process) != ERR);
    assert(sysobj_init(&dir->cwdObj, &dir->sysdir, "cwd", &cwdOps, process) != ERR);
    assert(sysobj_init(&dir->cmdlineObj, &dir->sysdir, "cmdline", &cmdlineOps, process) != ERR);
    assert(sysobj_init(&dir->noteObj, &dir->sysdir, "note", &noteOps, process) != ERR);
}

process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority)
{
    process_t* process = heap_alloc(sizeof(process_t), HEAP_NONE);
    if (process == NULL)
    {
        return ERRPTR(ENOMEM);
    }

    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    if (argv_init(&process->argv, argv) == ERR)
    {
        return ERRPTR(ENOMEM);
    }

    if (cwd != NULL)
    {
        vfs_ctx_init(&process->vfsCtx, cwd);
    }
    else if (parent != NULL)
    {
        path_t parentCwd;
        vfs_ctx_get_cwd(&parent->vfsCtx, &parentCwd);

        vfs_ctx_init(&process->vfsCtx, &parentCwd);

        path_put(&parentCwd);
    }
    else
    {
        vfs_ctx_init(&process->vfsCtx, NULL);
    }

    space_init(&process->space);
    wait_queue_init(&process->queue);
    futex_ctx_init(&process->futexCtx);
    process->threads.isDying = false;
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
    LOG_INFO("process: on free pid=%d\n", process->id);
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    heap_free(process);
}

void process_free(process_t* process)
{
    assert(list_is_empty(&process->threads.list));

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
    rwlock_init(&treeLock);

    LOG_INFO("process: init self dir\n");
    process_dir_init(&self, "self", NULL);

    LOG_INFO("process: create kernel process\n");
    kernelProcess = process_new(NULL, NULL, NULL, PRIORITY_MAX - 1);
    assert(kernelProcess != NULL);
}

process_t* process_get_kernel(void)
{
    return kernelProcess;
}