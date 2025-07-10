#include "process.h"

#include "fs/ctl.h"
#include "fs/file.h"
#include "fs/path.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sync/rwlock.h"

#include <_internal/MAX_PATH.h>
#include <assert.h>
#include <stdint.h>
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

static sysfs_group_t procGroup;

static process_t* process_file_get_process(file_t* file)
{
    process_t* process = file->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    if (process == kernelProcess)
    {
        errno = EACCES;
        return NULL;
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
        errno = EINVAL;
        return ERR;
    }
    if (prio >= PRIORITY_MAX_USER)
    {
        errno = EACCES;
        return ERR;
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

static uint64_t process_cwd_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    pathname_t cwd;

    lock_acquire(&process->vfsCtx.lock);
    if (path_to_name(&process->vfsCtx.cwd, &cwd) == ERR)
    {
        lock_release(&process->vfsCtx.lock);
        return ERR;
    }
    lock_release(&process->vfsCtx.lock);

    uint64_t length = strlen(cwd.string);
    return BUFFER_READ(buffer, count, offset, cwd.string, length);
}

static file_ops_t cwdOps = {
    .read = process_cwd_read,
};

static uint64_t process_cmdline_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
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
    return BUFFER_READ(buffer, count, offset, last, length);
}

static file_ops_t cmdlineOps = {
    .read = process_cmdline_read,
};

static uint64_t process_note_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
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
        errno = EINVAL;
        return ERR;
    }

    if (thread_send_note(thread, buffer, count) == ERR)
    {
        return ERR;
    }

    return count;
}

static file_ops_t noteOps = {
    .write = process_note_write,
};

static void process_inode_cleanup(inode_t* inode)
{
    process_t* process = inode->private;
    LOG_DEBUG("process: cleanup pid=%d\n", process->id);
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->queue);
    futex_ctx_deinit(&process->futexCtx);
    heap_free(process);
}

static inode_ops_t processInodeOps = {
    .cleanup = process_inode_cleanup,
};

static uint64_t process_dir_init(process_dir_t* dir, const char* name, process_t* process)
{
    if (sysfs_dir_init(&dir->dir, &procGroup.root, name, &processInodeOps, process) == ERR)
    {
        return ERR;
    }

    if (sysfs_file_init(&dir->ctlFile, &dir->dir, "ctl", NULL, &ctlOps, process) == ERR)
    {
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->cwdFile, &dir->dir, "cwd", NULL, &cwdOps, process) == ERR)
    {
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->cmdlineFile, &dir->dir, "cmdline", NULL, &cmdlineOps, process) == ERR)
    {
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->noteFile, &dir->dir, "note", NULL, &noteOps, process) == ERR)
    {
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    return 0;
}

process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority)
{
    process_t* process = heap_alloc(sizeof(process_t), HEAP_NONE);
    if (process == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    if (argv_init(&process->argv, argv) == ERR)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (cwd != NULL)
    {
        vfs_ctx_init(&process->vfsCtx, cwd);
    }
    else if (parent != NULL)
    {
        path_t parentCwd = PATH_EMPTY;
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

    LOG_INFO("process: new process created pid=%d parent=%d priority=%d\n", process->id, parent ? parent->id : 0,
        priority);
    return process;
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

    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_cleanup
    wait_unblock(&process->queue, WAIT_ALL);
    sysfs_dir_deinit(&process->dir.dir);
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

    LOG_INFO("process: init\n");

    assert(sysfs_group_init(&procGroup, PATHNAME("/proc")) != ERR);
    assert(process_dir_init(&self, "self", NULL) != ERR);

    LOG_INFO("process: create kernel process\n");
    kernelProcess = process_new(NULL, NULL, NULL, PRIORITY_MAX - 1);
    assert(kernelProcess != NULL);
}

process_t* process_get_kernel(void)
{
    return kernelProcess;
}
