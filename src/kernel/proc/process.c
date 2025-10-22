#include "process.h"

#include "fs/file.h"
#include "fs/path.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/vmm.h"
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

// TODO: Reimplement "self" using a bind.

static process_t kernelProcess;
static bool kernelProcessInitalized = false;

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

// Should be acquired whenever a process tree is being read or modified.
static rwlock_t treeLock = RWLOCK_CREATE;

static mount_t* mount;

static process_t* process_file_get_process(file_t* file)
{
    process_t* process = file->inode->private;
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (process == &kernelProcess)
    {
        errno = EACCES;
        return NULL;
    }

    return process;
}

static uint64_t process_prio_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    priority_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return BUFFER_READ(buffer, count, offset, prioStr, length);
}

static uint64_t process_prio_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)offset; // Unused

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    char prioStr[MAX_NAME];
    if (count >= MAX_NAME)
    {
        errno = EINVAL;
        return ERR;
    }

    memcpy(prioStr, buffer, count);
    prioStr[count] = '\0';

    long long int prio = atoll(prioStr);
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
    return count;
}

static file_ops_t prioOps = {
    .read = process_prio_read,
    .write = process_prio_write,
};

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

    uint64_t length;
    const char* strings = argv_get_strings(&process->argv, &length);
    return BUFFER_READ(buffer, count, offset, strings, length);
}

static file_ops_t cmdlineOps = {
    .read = process_cmdline_read,
};

static uint64_t process_note_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)offset; // Unused

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
    DEREF(process);
}

static inode_ops_t inodeOps = {
    .cleanup = process_inode_cleanup,
};

static uint64_t process_dir_init(process_t* process, const char* name)
{
    process->dir = sysfs_dir_new(mount->root, name, &inodeOps, REF(process));
    if (process->dir == NULL)
    {
        return ERR;
    }

    process->prioFile = sysfs_file_new(process->dir, "prio", &inodeOps, &prioOps, REF(process));
    if (process->prioFile == NULL)
    {
        DEREF(process->dir);
        return ERR;
    }

    process->cwdFile = sysfs_file_new(process->dir, "cwd", &inodeOps, &cwdOps, REF(process));
    if (process->cwdFile == NULL)
    {
        DEREF(process->dir);
        return ERR;
    }

    process->cmdlineFile = sysfs_file_new(process->dir, "cmdline", &inodeOps, &cmdlineOps, REF(process));
    if (process->cmdlineFile == NULL)
    {
        DEREF(process->dir);
        return ERR;
    }

    process->noteFile = sysfs_file_new(process->dir, "note", &inodeOps, &noteOps, REF(process));
    if (process->noteFile == NULL)
    {
        DEREF(process->dir);
        return ERR;
    }

    process->statusFile = sysfs_file_new(process->dir, "status", &inodeOps, &statusOps, REF(process));
    if (process->statusFile == NULL)
    {
        DEREF(process->dir);
        return ERR;
    }

    return 0;
}

static void process_free(process_t* process)
{
    LOG_DEBUG("freeing process pid=%d\n", process->id);
    assert(list_is_empty(&process->threads.list));

    if (atomic_load(&process->isDying) == false)
    {
        process_kill(process, EXIT_SUCCESS);
    }

    process_t* parent = process->parent;

    rwlock_write_acquire(&treeLock);
    list_remove(&process->parent->children, &process->entry);
    process->parent = NULL;

    process_t* child;
    process_t* temp;
    LIST_FOR_EACH_SAFE(child, temp, &process->children, entry)
    {
        list_remove(&process->children, &child->entry);
        child->parent = NULL;
    }
    rwlock_write_release(&treeLock);

    DEREF(parent);

    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->dyingWaitQueue);
    futex_ctx_deinit(&process->futexCtx);
    heap_free(process);
}

static uint64_t process_init(process_t* process, process_t* parent, const char** argv, const path_t* cwd,
    priority_t priority)
{
    ref_init(&process->ref, process_free);
    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    atomic_init(&process->status, EXIT_SUCCESS);

    if (argv_init(&process->argv, argv) == ERR)
    {
        return ERR;
    }

    if (namespace_init(&process->namespace, parent != NULL ? &parent->namespace : NULL, process) == ERR)
    {
        argv_deinit(&process->argv);
        return ERR;
    }

    if (space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
            SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY) == ERR)
    {
        namespace_deinit(&process->namespace);
        argv_deinit(&process->argv);
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
            argv_deinit(&process->argv);
            namespace_deinit(&process->namespace);
            space_deinit(&process->space);
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
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

    list_entry_init(&process->entry);
    list_init(&process->children);
    if (parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&treeLock);
        list_push(&parent->children, &process->entry);
        process->parent = REF(parent);
    }
    else
    {
        process->parent = NULL;
    }

    process->dir = NULL;
    process->prioFile = NULL;
    process->cwdFile = NULL;
    process->cmdlineFile = NULL;
    process->noteFile = NULL;
    process->statusFile = NULL;

    assert(process == &kernelProcess || process_is_child(process, kernelProcess.id));

    LOG_DEBUG("new pid=%d parent=%d priority=%d\n", process->id, parent ? parent->id : 0, priority);
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
    if (process_dir_init(process, name) == ERR)
    {
        DEREF(process);
        return NULL;
    }

    return process;
}

void process_kill(process_t* process, uint64_t status)
{
    LOG_DEBUG("killing process pid=%d with status=%llu refCount=%d\n", process->id, status, process->ref.count);
    LOCK_SCOPE(&process->threads.lock);

    if (atomic_exchange(&process->isDying, true))
    {
        return;
    }

    atomic_store(&process->status, status);

    uint64_t killCount = 0;
    thread_t* thread;
    LIST_FOR_EACH(thread, &process->threads.list, processEntry)
    {
        thread_send_note(thread, "kill", 4);
        killCount++;
    }

    if (killCount > 0)
    {
        LOG_DEBUG("sent kill note to %llu threads in process pid=%d\n", killCount, process->id);
    }

    vfs_ctx_deinit(&process->vfsCtx); // Here instead of in process_inode_cleanup, makes sure that files close
                                      // immediately to notify blocking threads.
    // The dir entries have refs to the process, so we must deinit them here.
    DEREF(process->dir);
    DEREF(process->prioFile);
    DEREF(process->cwdFile);
    DEREF(process->cmdlineFile);
    DEREF(process->noteFile);
    DEREF(process->statusFile);
    wait_unblock(&process->dyingWaitQueue, WAIT_ALL, EOK);
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

void process_procfs_init(void)
{
    mount = sysfs_mount_new(NULL, "proc", NULL, NULL);
    if (mount == NULL)
    {
        panic(NULL, "Failed to mount /proc filesystem");
    }

    assert(kernelProcessInitalized);

    // Kernel process was created before sysfs was initialized, so we have to delay this until now.
    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "%d", kernelProcess.id);
    if (process_dir_init(&kernelProcess, name) == ERR)
    {
        panic(NULL, "Failed to create /proc/[pid] directory for kernel process");
    }
}

process_t* process_get_kernel(void)
{
    if (!kernelProcessInitalized)
    {
        if (process_init(&kernelProcess, NULL, NULL, NULL, PRIORITY_MAX - 1) == ERR)
        {
            panic(NULL, "Failed to init kernel process");
        }
        LOG_INFO("kernel process initialized with pid=%d\n", kernelProcess.id);
        kernelProcessInitalized = true;
    }

    return &kernelProcess;
}

SYSCALL_DEFINE(SYS_GETPID, pid_t)
{
    return sched_process()->id;
}
