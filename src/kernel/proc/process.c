#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/proc/reaper.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static process_t* kernelProcess = NULL;

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

static mount_t* proc = NULL;
static dentry_t* self = NULL;

static uint64_t process_self_readlink(inode_t* inode, char* buffer, uint64_t count)
{
    (void)inode; // Unused

    process_t* process = sched_process();
    int ret = snprintf(buffer, count, "%llu", process->id);
    if (ret < 0 || ret >= (int)count)
    {
        return ERR;
    }

    return ret;
}

static inode_ops_t selfOps = {
    .readlink = process_self_readlink,
};

static uint64_t process_prio_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;

    priority_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return BUFFER_READ(buffer, count, offset, prioStr, length);
}

static uint64_t process_prio_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)offset; // Unused

    process_t* process = file->inode->private;

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
    if (prio > PRIORITY_MAX_USER)
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
    process_t* process = file->inode->private;

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    pathname_t cwdName;
    if (path_to_name(&cwd, &cwdName) == ERR)
    {
        return ERR;
    }

    uint64_t length = strlen(cwdName.string);
    return BUFFER_READ(buffer, count, offset, cwdName.string, length);
}

static uint64_t process_cwd_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)offset; // Unused

    process_t* process = file->inode->private;

    char cwdStr[MAX_PATH];
    if (count >= MAX_PATH)
    {
        errno = EINVAL;
        return ERR;
    }

    memcpy(cwdStr, buffer, count);
    cwdStr[count] = '\0';

    pathname_t cwdPathname;
    if (pathname_init(&cwdPathname, cwdStr) == ERR)
    {
        return ERR;
    }

    path_t path = cwd_get(&process->cwd);
    PATH_DEFER(&path);

    if (path_walk(&path, &cwdPathname, &process->ns) == ERR)
    {
        return ERR;
    }

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        errno = ENOENT;
        return ERR;
    }

    cwd_set(&process->cwd, &path);
    return count;
}

static file_ops_t cwdOps = {
    .read = process_cwd_read,
    .write = process_cwd_write,
};

static uint64_t process_cmdline_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;

    if (process->argv == NULL || process->argc == 0)
    {
        return 0;
    }

    uint64_t totalSize = 0;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        totalSize += strlen(process->argv[i]) + 1;
    }

    if (totalSize == 0)
    {
        return 0;
    }

    char* cmdline = malloc(totalSize);
    if (cmdline == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    char* dest = cmdline;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        uint64_t len = strlen(process->argv[i]) + 1;
        memcpy(dest, process->argv[i], len);
        dest += len;
    }

    uint64_t result = BUFFER_READ(buffer, count, offset, cmdline, totalSize);
    free(cmdline);
    return result;
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

    if (count >= NOTE_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    LOCK_SCOPE(&process->threads.lock);

    thread_t* thread = CONTAINER_OF_SAFE(list_first(&process->threads.list), thread_t, processEntry);
    if (thread == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, count);
    if (thread_send_note(thread, string) == ERR)
    {
        return ERR;
    }

    return count;
}

static file_ops_t noteOps = {
    .write = process_note_write,
};

static uint64_t process_notegroup_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)offset; // Unused

    if (count == 0)
    {
        return 0;
    }

    if (count >= NOTE_MAX)
    {
        errno = EINVAL;
        return ERR;
    }
    process_t* process = file->inode->private;

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, count);
    if (group_send_note(&process->group, string) == ERR)
    {
        return ERR;
    }

    return count;
}

static file_ops_t notegroupOps = {
    .write = process_notegroup_write,
};

static uint64_t process_gid_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;

    char gidStr[MAX_NAME];
    uint32_t length = snprintf(gidStr, MAX_NAME, "%llu", group_get_id(&process->group));
    return BUFFER_READ(buffer, count, offset, gidStr, length);
}

static file_ops_t gidOps = {
    .read = process_gid_read,
};

static uint64_t process_pid_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;

    char pidStr[MAX_NAME];
    uint32_t length = snprintf(pidStr, MAX_NAME, "%llu", process->id);
    return BUFFER_READ(buffer, count, offset, pidStr, length);
}

static file_ops_t pidOps = {
    .read = process_pid_read,
};

static uint64_t process_wait_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;

    if (WAIT_BLOCK(&process->dyingQueue, atomic_load(&process->flags) & PROCESS_DYING) == ERR)
    {
        return ERR;
    }

    lock_acquire(&process->status.lock);
    uint64_t result = BUFFER_READ(buffer, count, offset, process->status.buffer, strlen(process->status.buffer));
    lock_release(&process->status.lock);
    return result;
}

static wait_queue_t* process_wait_poll(file_t* file, poll_events_t* revents)
{
    process_t* process = file->inode->private;
    if (atomic_load(&process->flags) & PROCESS_DYING)
    {
        *revents |= POLLIN;
        return &process->dyingQueue;
    }

    return &process->dyingQueue;
}

static file_ops_t waitOps = {
    .read = process_wait_read,
    .poll = process_wait_poll,
};

static uint64_t process_perf_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = file->inode->private;
    uint64_t userPages = space_user_page_count(&process->space);

    lock_acquire(&process->threads.lock);
    uint64_t threadCount = list_length(&process->threads.list);
    lock_release(&process->threads.lock);

    clock_t userClocks = atomic_load(&process->perf.userClocks);
    clock_t kernelClocks = atomic_load(&process->perf.kernelClocks);
    clock_t startTime = process->perf.startTime;

    char statStr[MAX_PATH];
    int length = snprintf(statStr, sizeof(statStr),
        "user_clocks %llu\nkernel_sched_clocks %llu\nstart_clocks %llu\nuser_pages %llu\nthread_count %llu", userClocks,
        kernelClocks, startTime, userPages, threadCount);
    if (length < 0)
    {
        errno = EIO;
        return ERR;
    }

    return BUFFER_READ(buffer, count, offset, statStr, (uint64_t)length);
}

static file_ops_t perfOps = {
    .read = process_perf_read,
};

static uint64_t process_ns_open(file_t* file)
{
    process_t* process = file->inode->private;

    namespace_handle_t* handle = malloc(sizeof(namespace_handle_t));
    if (handle == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    if (namespace_handle_init(handle, &process->ns, NAMESPACE_HANDLE_SHARE) == ERR)
    {
        free(handle);
        return ERR;
    }

    file->private = handle;
    return 0;
}

static void process_ns_close(file_t* file)
{
    if (file->private == NULL)
    {
        return;
    }

    namespace_handle_t* handle = file->private;
    namespace_handle_deinit(handle);
    free(handle);
    file->private = NULL;
}

static file_ops_t nsOps = {
    .open = process_ns_open,
    .close = process_ns_close,
};

static uint64_t process_ctl_close(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2 && argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    if (argc == 2)
    {
        fd_t fd;
        if (sscanf(argv[1], "%lld", &fd) != 1)
        {
            errno = EINVAL;
            return ERR;
        }

        if (file_table_close(&process->fileTable, fd) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        fd_t minFd;
        if (sscanf(argv[1], "%lld", &minFd) != 1)
        {
            errno = EINVAL;
            return ERR;
        }

        fd_t maxFd;
        if (sscanf(argv[2], "%lld", &maxFd) != 1)
        {
            errno = EINVAL;
            return ERR;
        }

        if (file_table_close_range(&process->fileTable, minFd, maxFd) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

static uint64_t process_ctl_dup2(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    fd_t oldFd;
    if (sscanf(argv[1], "%lld", &oldFd) != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    fd_t newFd;
    if (sscanf(argv[2], "%lld", &newFd) != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file_table_dup2(&process->fileTable, oldFd, newFd) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t process_ctl_join(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    fd_t fd;
    if (sscanf(argv[1], "%llu", &fd) != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    file_t* nsFile = file_table_get(&process->fileTable, fd);
    if (nsFile == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(nsFile);

    if (nsFile->ops != &nsOps)
    {
        errno = EINVAL;
        return ERR;
    }

    namespace_handle_t* target = nsFile->private;
    if (target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    namespace_handle_deinit(&process->ns);

    if (namespace_handle_init(&process->ns, target, NAMESPACE_HANDLE_SHARE) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t process_ctl_start(file_t* file, uint64_t argc, const char** argv)
{
    (void)argv; // Unused

    if (argc != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    atomic_fetch_and(&process->flags, ~PROCESS_SUSPENDED);
    wait_unblock(&process->suspendQueue, WAIT_ALL, EOK);

    return 0;
}

static uint64_t process_ctl_kill(file_t* file, uint64_t argc, const char** argv)
{
    (void)argv; // Unused

    if (argc != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->private;

    process_kill(process, 0);

    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {
        {"close", process_ctl_close, 2, 3},
        {"dup2", process_ctl_dup2, 3, 3},
        {"join", process_ctl_join, 2, 2},
        {"start", process_ctl_start, 1, 1},
        {"kill", process_ctl_kill, 1, 1},
        {0},
    })

static uint64_t process_env_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    if (file->inode->private == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, file->inode->private, file->inode->size);
}

static uint64_t process_env_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    uint64_t requiredSize = *offset + count + 1;
    if (requiredSize > file->inode->size)
    {
        void* newData = realloc(file->inode->private, requiredSize);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + file->inode->size, 0, requiredSize - file->inode->size);
        file->inode->private = newData;
        file->inode->size = requiredSize;
    }

    return BUFFER_WRITE(file->inode->private, count, offset, buffer, file->inode->size);
}

static file_ops_t envFileOps = {
    .read = process_env_read,
    .write = process_env_write,
    .seek = file_generic_seek,
};

static uint64_t process_env_create(inode_t* dir, dentry_t* target, mode_t mode);
static uint64_t process_env_remove(inode_t* parent, dentry_t* target);
static void process_env_cleanup(inode_t* inode);

static inode_ops_t envFileInodeOps = {
    .cleanup = process_env_cleanup,
};

static inode_ops_t envInodeOps = {
    .create = process_env_create,
    .remove = process_env_remove,
};

static uint64_t process_env_create(inode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&dir->mutex);

    process_t* process = dir->private;
    assert(process != NULL);

    inode_t* inode = inode_new(dir->superblock, vfs_id_get(), INODE_FILE, &envFileInodeOps, &envFileOps);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);

    inode->private = NULL;
    inode->size = 0;

    dentry_make_positive(target, inode);

    lock_acquire(&process->dir.lock);
    list_push_back(&process->dir.envEntries, &REF(target)->otherEntry);
    lock_release(&process->dir.lock);

    return 0;
}

static uint64_t process_env_remove(inode_t* dir, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    process_t* process = dir->private;
    assert(process != NULL);

    lock_acquire(&process->dir.lock);
    list_remove(&process->dir.envEntries, &target->otherEntry);
    UNREF(target);
    lock_release(&process->dir.lock);

    return 0;
}

static void process_env_cleanup(inode_t* inode)
{
    if (inode->private != NULL)
    {
        free(inode->private);
        inode->private = NULL;
        inode->size = 0;
    }
}

static void process_proc_cleanup(inode_t* inode)
{
    process_t* process = inode->private;
    if (process == NULL)
    {
        return;
    }

    LOG_DEBUG("freeing process pid=%d\n", process->id);

    assert(list_is_empty(&process->threads.list));
    assert(process->dir.env == NULL);
    assert(list_is_empty(&process->dir.files));
    assert(list_is_empty(&process->dir.envEntries));

    if (process->argv != NULL)
    {
        for (uint64_t i = 0; i < process->argc; i++)
        {
            if (process->argv[i] != NULL)
            {
                free(process->argv[i]);
            }
        }
        free(process->argv);
        process->argv = NULL;
        process->argc = 0;
    }

    group_member_deinit(&process->group);
    cwd_deinit(&process->cwd);
    file_table_deinit(&process->fileTable);
    namespace_handle_deinit(&process->ns);
    space_deinit(&process->space);
    wait_queue_deinit(&process->dyingQueue);
    wait_queue_deinit(&process->suspendQueue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static inode_ops_t procInodeOps = {
    .cleanup = process_proc_cleanup,
};

static sysfs_file_desc_t files[] = {
    {.name = "prio", .inodeOps = NULL, .fileOps = &prioOps},
    {.name = "cwd", .inodeOps = NULL, .fileOps = &cwdOps},
    {.name = "cmdline", .inodeOps = NULL, .fileOps = &cmdlineOps},
    {.name = "note", .inodeOps = NULL, .fileOps = &noteOps},
    {.name = "notegroup", .inodeOps = NULL, .fileOps = &notegroupOps},
    {.name = "gid", .inodeOps = NULL, .fileOps = &gidOps},
    {.name = "pid", .inodeOps = NULL, .fileOps = &pidOps},
    {.name = "wait", .inodeOps = NULL, .fileOps = &waitOps},
    {.name = "perf", .inodeOps = NULL, .fileOps = &perfOps},
    {.name = "ns", .inodeOps = NULL, .fileOps = &nsOps},
    {.name = "ctl", .inodeOps = NULL, .fileOps = &ctlOps},
    {0},
};

static uint64_t process_dir_init(process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char name[MAX_NAME];
    int ret = snprintf(name, MAX_NAME, "%llu", process->id);
    if (ret < 0 || ret >= MAX_NAME)
    {
        return ERR;
    }

    path_t procPath = PATH_CREATE(proc, proc->source);
    PATH_DEFER(&procPath);

    process->dir.dir = sysfs_submount_new(&procPath, name, &process->ns, MODE_PARENTS | MODE_PRIVATE | MODE_ALL_PERMS, &procInodeOps, NULL, process);
    if (process->dir.dir == NULL)
    {
        return ERR;
    }

    process->dir.env = sysfs_dir_new(process->dir.dir->source, "env", &envInodeOps, process);
    if (process->dir.env == NULL)
    {
        process_dir_deinit(process);
        return ERR;
    }

    if (sysfs_files_create(process->dir.dir->source, files, process, &process->dir.files) == ERR)
    {
        process_dir_deinit(process);
        return ERR;
    }

    return 0;
}

process_t* process_new(priority_t priority, gid_t gid, namespace_handle_t* source, namespace_handle_flags_t flags)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    process->id = atomic_fetch_add(&newPid, 1);
    group_member_init(&process->group);
    atomic_init(&process->priority, priority);
    memset_s(process->status.buffer, PROCESS_STATUS_MAX, 0, PROCESS_STATUS_MAX);
    lock_init(&process->status.lock);

    if (space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
            SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY) == ERR)
    {
        free(process);
        return NULL;
    }

    if (namespace_handle_init(&process->ns, source, flags) == ERR)
    {
        space_deinit(&process->space);
        free(process);
        return NULL;
    }

    cwd_init(&process->cwd);
    file_table_init(&process->fileTable);
    futex_ctx_init(&process->futexCtx);
    perf_process_ctx_init(&process->perf);
    note_handler_init(&process->noteHandler);
    wait_queue_init(&process->suspendQueue);
    wait_queue_init(&process->dyingQueue);
    atomic_init(&process->flags, PROCESS_NONE);

    process->threads.newTid = 0;
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

    list_entry_init(&process->zombieEntry);

    process->dir.dir = NULL;
    list_init(&process->dir.files);
    process->dir.env = NULL;
    list_init(&process->dir.envEntries);
    lock_init(&process->dir.lock);

    process->argv = NULL;
    process->argc = 0;

    if (group_add(gid, &process->group) == ERR)
    {
        process_kill(process, "init failed");
        return NULL;
    }

    if (process->id != 0) // Delay kernel process /proc dir init
    {
        if (process_dir_init(process) == ERR)
        {
            process_kill(process, "init failed");
            return NULL;
        }
    }

    LOG_DEBUG("created process pid=%d\n", process->id);
    return process;
}

void process_kill(process_t* process, const char* status)
{
    lock_acquire(&process->threads.lock);

    if (atomic_fetch_or(&process->flags, PROCESS_DYING) & PROCESS_DYING)
    {
        lock_release(&process->threads.lock);
        return;
    }

    lock_acquire(&process->status.lock);
    strncpy_s(process->status.buffer, PROCESS_STATUS_MAX, status, PROCESS_STATUS_MAX - 1);
    process->status.buffer[PROCESS_STATUS_MAX - 1] = '\0';
    lock_release(&process->status.lock);

    uint64_t killCount = 0;
    thread_t* thread;
    LIST_FOR_EACH(thread, &process->threads.list, processEntry)
    {
        thread_send_note(thread, "kill");
        killCount++;
    }

    if (killCount > 0)
    {
        LOG_DEBUG("sent kill note to %llu threads in process pid=%d\n", killCount, process->id);
    }

    lock_release(&process->threads.lock);

    // Anything that another process could be waiting on must be cleaned up here.

    namespace_unmount(&process->ns, process->dir.dir, MODE_PARENTS);

    cwd_clear(&process->cwd);
    file_table_close_all(&process->fileTable);
    namespace_handle_clear(&process->ns);
    group_remove(&process->group);

    wait_unblock(&process->dyingQueue, WAIT_ALL, EOK);

    reaper_push(process);
}

void process_dir_deinit(process_t* process)
{
    if (process == NULL)
    {
        return;
    }

    LOCK_SCOPE(&process->dir.lock);

    while (!list_is_empty(&process->dir.envEntries))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_first(&process->dir.envEntries), dentry_t, otherEntry));
    }

    while (!list_is_empty(&process->dir.files))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_first(&process->dir.files), dentry_t, otherEntry));
    }

    if (process->dir.env != NULL)
    {
        UNREF(process->dir.env);
        process->dir.env = NULL;
    }

    if (process->dir.dir != NULL)
    {
        UNREF(process->dir.dir);
        process->dir.dir = NULL;
    }
}

uint64_t process_copy_env(process_t* dest, process_t* src)
{
    if (dest == NULL || src == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&src->dir.lock);
    LOCK_SCOPE(&dest->dir.lock);

    if (!list_is_empty(&dest->dir.envEntries))
    {
        errno = EBUSY;
        return ERR;
    }

    superblock_t* superblock = dest->dir.env->inode->superblock;

    dentry_t* srcDentry;
    LIST_FOR_EACH(srcDentry, &src->dir.envEntries, otherEntry)
    {
        inode_t* inode = inode_new(superblock, vfs_id_get(), INODE_FILE, &envFileInodeOps, &envFileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);

        MUTEX_SCOPE(&srcDentry->inode->mutex);
        if (srcDentry->inode->private != NULL)
        {
            inode->private = malloc(srcDentry->inode->size);
            if (inode->private == NULL)
            {
                return ERR;
            }
            memcpy(inode->private, srcDentry->inode->private, srcDentry->inode->size);
            inode->size = srcDentry->inode->size;
        }

        dentry_t* dentry = dentry_new(superblock, dest->dir.env, srcDentry->name);
        if (dentry == NULL)
        {
            return ERR;
        }

        dentry_make_positive(dentry, inode);
        list_push_back(&dest->dir.envEntries, &dentry->otherEntry);
    }

    return 0;
}

uint64_t process_set_cmdline(process_t* process, char** argv, uint64_t argc)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (argv == NULL || argc == 0)
    {
        process->argv = NULL;
        process->argc = 0;
        return 0;
    }

    char** newArgv = malloc(sizeof(char*) * (argc + 1));
    if (newArgv == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    uint64_t i = 0;
    for (; i < argc; i++)
    {
        if (argv[i] == NULL)
        {
            break;
        }
        size_t len = strlen(argv[i]) + 1;
        newArgv[i] = malloc(len);
        if (newArgv[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(newArgv[j]);
            }
            free(newArgv);
            errno = ENOMEM;
            return ERR;
        }
        memcpy(newArgv[i], argv[i], len);
    }
    newArgv[i] = NULL;

    uint64_t newArgc = i;
    if (process->argv != NULL)
    {
        for (uint64_t j = 0; j < process->argc; j++)
        {
            if (process->argv[j] != NULL)
            {
                free(process->argv[j]);
            }
        }
        free(process->argv);
    }

    process->argv = newArgv;
    process->argc = newArgc;

    return 0;
}

bool process_has_thread(process_t* process, tid_t tid)
{
    LOCK_SCOPE(&process->threads.lock);

    thread_t* thread;
    LIST_FOR_EACH(thread, &process->threads.list, processEntry)
    {
        if (thread->id == tid)
        {
            return true;
        }
    }

    return false;
}

process_t* process_get_kernel(void)
{
    if (kernelProcess == NULL)
    {
        kernelProcess = process_new(PRIORITY_MAX, GID_NONE, NULL, NAMESPACE_HANDLE_SHARE);
        if (kernelProcess == NULL)
        {
            panic(NULL, "Failed to create kernel process");
        }
        LOG_INFO("kernel process initialized with pid=%d\n", kernelProcess->id);
    }

    return kernelProcess;
}

void process_procfs_init(void)
{
    proc = sysfs_mount_new("proc", NULL, MODE_ALL_PERMS, NULL, NULL, NULL);
    if (proc == NULL)
    {
        panic(NULL, "Failed to mount /proc");
    }

    self = sysfs_symlink_new(proc->source, "self", &selfOps, NULL);
    if (self == NULL)
    {
        panic(NULL, "Failed to create /proc/self symlink");
    }

    assert(kernelProcess != NULL);

    // Kernel process was created before sysfs was initialized, so we have to delay this until now.
    if (process_dir_init(kernelProcess) == ERR)
    {
        panic(NULL, "Failed to create /proc/[pid] directory for kernel process");
    }
}

SYSCALL_DEFINE(SYS_GETPID, pid_t)
{
    return sched_process()->id;
}
