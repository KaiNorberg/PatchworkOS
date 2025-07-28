#include "process.h"

#include "fs/ctl.h"
#include "fs/file.h"
#include "fs/path.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "sync/rwlock.h"

#include <_internal/MAX_PATH.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static process_t kernelProcess;

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock = RWLOCK_CREATE();

static process_dir_t self;
static sysfs_group_t procGroup;

static process_t* process_file_get_process(file_t* file)
{
    process_t* process = file->inode->private;
    if (process == NULL)
    {
        process = sched_process();
    }

    if (process == &kernelProcess)
    {
        errno = EACCES;
        return NULL;
    }

    return process;
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

CTL_STANDARD_OPS_DEFINE(ctlOps,
    (ctl_array_t){
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

    path_t cwd = PATH_EMPTY;
    if (vfs_ctx_get_cwd(&process->vfsCtx, &cwd) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&cwd);

    pathname_t cwdName;
    if (path_to_name(&process->vfsCtx.cwd, &cwdName) == ERR)
    {
        return ERR;
    }

    uint64_t length = strlen(cwdName.string);
    return BUFFER_READ(buffer, count, offset, cwdName.string, length);
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

    LOCK_SCOPE(&process->threads.lock);

    thread_t* thread = CONTAINER_OF_SAFE(list_first(&process->threads.aliveThreads), thread_t, processEntry);
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

static uint64_t process_status_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    WAIT_BLOCK(&process->dyingWaitQueue, atomic_load(&process->isDying));

    char status[MAX_PATH];
    snprintf(status, sizeof(status), "%llu", atomic_load(&process->status));

    return BUFFER_READ(buffer, count, offset, status, strlen(status));
}

static file_ops_t statusOps = {
    .read = process_status_read,
};

static void process_inode_cleanup(inode_t* inode)
{
    process_t* process = inode->private;
    LOG_DEBUG("cleaning up process pid=%d\n", process->id);
    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->dyingWaitQueue);
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
        sysfs_file_deinit(&dir->ctlFile);
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->cmdlineFile, &dir->dir, "cmdline", NULL, &cmdlineOps, process) == ERR)
    {
        sysfs_file_deinit(&dir->ctlFile);
        sysfs_file_deinit(&dir->cwdFile);
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->noteFile, &dir->dir, "note", NULL, &noteOps, process) == ERR)
    {
        sysfs_file_deinit(&dir->ctlFile);
        sysfs_file_deinit(&dir->cwdFile);
        sysfs_file_deinit(&dir->cmdlineFile);
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    if (sysfs_file_init(&dir->statusFile, &dir->dir, "status", NULL, &statusOps, process) == ERR)
    {
        sysfs_file_deinit(&dir->noteFile);
        sysfs_file_deinit(&dir->ctlFile);
        sysfs_file_deinit(&dir->cwdFile);
        sysfs_file_deinit(&dir->cmdlineFile);
        sysfs_dir_deinit(&dir->dir);
        return ERR;
    }

    return 0;
}

static void process_dir_deinit(process_dir_t* dir)
{
    sysfs_file_deinit(&dir->ctlFile);
    sysfs_file_deinit(&dir->cwdFile);
    sysfs_file_deinit(&dir->cmdlineFile);
    sysfs_file_deinit(&dir->noteFile);
    sysfs_file_deinit(&dir->statusFile);
    sysfs_dir_deinit(&dir->dir);
}

static uint64_t process_init(process_t* process, process_t* parent, const char** argv, const path_t* cwd,
    priority_t priority)
{
    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    atomic_init(&process->status, EXIT_SUCCESS);
    if (argv_init(&process->argv, argv) == ERR)
    {
        return ERR;
    }

    if (space_init(&process->space) == ERR)
    {
        return ERR;
    }

    if (cwd != NULL)
    {
        vfs_ctx_init(&process->vfsCtx, cwd);
    }
    else if (parent != NULL)
    {
        path_t parentCwd = PATH_EMPTY;
        if (vfs_ctx_get_cwd(&parent->vfsCtx, &parentCwd) == ERR)
        {
            return ERR;
        }

        vfs_ctx_init(&process->vfsCtx, &parentCwd);

        path_put(&parentCwd);
    }
    else
    {
        vfs_ctx_init(&process->vfsCtx, NULL);
    }

    futex_ctx_init(&process->futexCtx);
    wait_queue_init(&process->dyingWaitQueue);
    atomic_init(&process->isDying, false);
    process->threads.newTid = 0;
    list_init(&process->threads.aliveThreads);
    list_init(&process->threads.zombieThreads);
    lock_init(&process->threads.lock);

    list_entry_init(&process->entry);
    list_init(&process->children);
    if (parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&treeLock);
        list_push(&parent->children, &process->entry);
        process->parent = parent;
    }
    else
    {
        process->parent = NULL;
    }

    LOG_INFO("new pid=%d parent=%d priority=%d\n", process->id, parent ? parent->id : 0, priority);
    return 0;
}

process_t* process_new(process_t* parent, const char** argv, const path_t* cwd, priority_t priority)
{
    process_t* process = heap_alloc(sizeof(process_t), HEAP_NONE);
    if (process == NULL)
    {
        return NULL;
    }

    if (process_init(process, parent, argv, cwd, priority) == ERR)
    {
        heap_free(process);
        return NULL;
    }

    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "%d", process->id);
    if (process_dir_init(&process->dir, name, process))
    {
        heap_free(process);
        return NULL;
    }

    return process;
}

void process_free(process_t* process)
{
    LOG_INFO("freeing process pid=%d\n", process->id);
    assert(list_is_empty(&process->threads.aliveThreads));
    assert(list_is_empty(&process->threads.zombieThreads));
    assert(atomic_load(&process->isDying));

    if (process->parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&treeLock);
        list_remove(&process->parent->children, &process->entry);

        process_t* child;
        process_t* temp;
        LIST_FOR_EACH_SAFE(child, temp, &process->children, entry)
        {
            list_remove(&process->children, &child->entry);
            child->parent = NULL;
        }
        process->parent = NULL;
    }

    process_dir_deinit(&process->dir);
}

void process_kill(process_t* process, uint64_t status)
{
    LOCK_SCOPE(&process->threads.lock);

    if (atomic_exchange(&process->isDying, true))
    {
        return;
    }
    
    uint64_t killCount = 0;
    thread_t* thread;
    LIST_FOR_EACH(thread, &process->threads.aliveThreads, processEntry)
    {
        const char* note = "kill";
        uint64_t noteLen = 4;
        thread_send_note(thread, note, noteLen);
        killCount++;
    }

    if (killCount > 0)
    {
        LOG_DEBUG("sent kill note to %llu threads in process pid=%d\n", killCount, process->id);
    }

    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_inode_cleanup, makes sure that files close immediately to notify blocking threads.
    wait_unblock(&process->dyingWaitQueue, WAIT_ALL);
}

bool process_is_child(process_t* process, pid_t parentId)
{
    RWLOCK_READ_SCOPE(&treeLock);

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

void process_kernel_init(void)
{
    if (process_init(&kernelProcess, NULL, NULL, NULL, PRIORITY_MAX - 1) == ERR)
    {
        panic(NULL, "Failed to init kernel process");
    }
    LOG_INFO("kernel process initialized with pid=%d\n", kernelProcess.id);
}

void process_procfs_init(void)
{
    if (sysfs_group_init(&procGroup, PATHNAME("/proc")) == ERR)
    {
        panic(NULL, "Failed to initialize process sysfs group");
    }
    if (process_dir_init(&self, "self", NULL) == ERR)
    {
        panic(NULL, "Failed to initialize process sysfs directory");
    }

    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "%d", kernelProcess.id);
    if (process_dir_init(&kernelProcess.dir, "", &kernelProcess) == ERR)
    {
        panic(NULL, "Failed to initialize kernel process sysfs directory");
    }
}

process_t* process_get_kernel(void)
{
    return &kernelProcess;
}

SYSCALL_DEFINE(SYS_GETPID, pid_t)
{
    return sched_process()->id;
}
