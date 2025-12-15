#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/proc/reaper.h>
#include <kernel/sync/rwlock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static process_t* kernelProcess = NULL;

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

static mount_t* procMount = NULL;
static dentry_t* selfDir = NULL;

static process_t* process_file_get_process(file_t* file)
{
    process_t* process = file->inode->private;
    if (process == NULL)
    {
        LOG_DEBUG("process_file_get_process: inode private is NULL\n");
        errno = EINVAL;
        return NULL;
    }

    if (process == kernelProcess)
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
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

    if (!dentry_is_positive(path.dentry))
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
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    if (process->cmdline == NULL || process->cmdlineSize == 0)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, process->cmdline, process->cmdlineSize);
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

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, MIN(count, NOTE_MAX - 1));
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

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, MIN(count, NOTE_MAX - 1));

    if (group_send_note(&process->groupEntry, string) == ERR)
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
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    char gidStr[MAX_NAME];
    uint32_t length = snprintf(gidStr, MAX_NAME, "%llu", group_get_id(&process->groupEntry));
    return BUFFER_READ(buffer, count, offset, gidStr, length);
}

static file_ops_t gidOps = {
    .read = process_gid_read,
};

static uint64_t process_wait_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    WAIT_BLOCK(&process->dyingQueue, atomic_load(&process->flags) & PROCESS_DYING);

    lock_acquire(&process->exitStatus.lock);
    uint64_t result =
        BUFFER_READ(buffer, count, offset, process->exitStatus.buffer, strlen(process->exitStatus.buffer));
    lock_release(&process->exitStatus.lock);
    return result;
}

static wait_queue_t* process_wait_poll(file_t* file, poll_events_t* revents)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return NULL;
    }

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

static uint64_t process_stat_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

static file_ops_t statOps = {
    .read = process_stat_read,
};

static uint64_t process_ctl_close(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2 && argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    if (argc == 2)
    {
        fd_t fd;
        if (sscanf(argv[1], "%lld", &fd) != 1)
        {
            errno = EINVAL;
            return ERR;
        }

        if (file_table_free(&process->fileTable, fd) == ERR)
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

        if (file_table_free_range(&process->fileTable, minFd, maxFd) == ERR)
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

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

static uint64_t process_ctl_start(file_t* file, uint64_t argc, const char** argv)
{
    (void)argv; // Unused

    if (argc != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

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

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    process_kill(process, 0);

    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {{"close", process_ctl_close, 2, 3}, {"dup2", process_ctl_dup2, 3, 3}, {"start", process_ctl_start, 1, 1},
        {"kill", process_ctl_kill, 1, 1}, {0}})

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
static uint64_t process_env_remove(inode_t* parent, dentry_t* target, mode_t mode);
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

static uint64_t process_env_remove(inode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        errno = EINVAL;
        return ERR;
    }

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
    assert(process->dir.self == NULL);
    assert(process->dir.dir == NULL);
    assert(process->dir.env == NULL);
    assert(list_is_empty(&process->dir.files));
    assert(list_is_empty(&process->dir.envEntries));

    if (process->cmdline != NULL)
    {
        free(process->cmdline);
        process->cmdline = NULL;
        process->cmdlineSize = 0;
    }

    group_entry_deinit(&process->groupEntry);
    cwd_deinit(&process->cwd);
    file_table_deinit(&process->fileTable);
    namespace_deinit(&process->ns);
    space_deinit(&process->space);
    wait_queue_deinit(&process->dyingQueue);
    wait_queue_deinit(&process->suspendQueue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static inode_ops_t procInodeOps = {
    .cleanup = process_proc_cleanup,
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

    process->dir.dir = sysfs_dir_new(procMount->source, name, &procInodeOps, process);
    if (process->dir.dir == NULL)
    {
        return ERR;
    }

    process->dir.env = sysfs_dir_new(process->dir.dir, "env", &envInodeOps, process);
    if (process->dir.env == NULL)
    {
        goto error;
    }

    dentry_t* prio = sysfs_file_new(process->dir.dir, "prio", NULL, &prioOps, process);
    if (prio == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &prio->otherEntry);

    dentry_t* cwd = sysfs_file_new(process->dir.dir, "cwd", NULL, &cwdOps, process);
    if (cwd == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &cwd->otherEntry);

    dentry_t* cmdline = sysfs_file_new(process->dir.dir, "cmdline", NULL, &cmdlineOps, process);
    if (cmdline == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &cmdline->otherEntry);

    dentry_t* note = sysfs_file_new(process->dir.dir, "note", NULL, &noteOps, process);
    if (note == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &note->otherEntry);

    dentry_t* notegroup = sysfs_file_new(process->dir.dir, "notegroup", NULL, &notegroupOps, process);
    if (notegroup == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &notegroup->otherEntry);

    dentry_t* gid = sysfs_file_new(process->dir.dir, "gid", NULL, &gidOps, process);
    if (gid == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &gid->otherEntry);

    dentry_t* wait = sysfs_file_new(process->dir.dir, "wait", NULL, &waitOps, process);
    if (wait == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &wait->otherEntry);

    dentry_t* perf = sysfs_file_new(process->dir.dir, "perf", NULL, &statOps, process);
    if (perf == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &perf->otherEntry);

    dentry_t* ctl = sysfs_file_new(process->dir.dir, "ctl", NULL, &ctlOps, process);
    if (ctl == NULL)
    {
        goto error;
    }
    list_push_back(&process->dir.files, &ctl->otherEntry);

    path_t selfPath = PATH_CREATE(procMount, selfDir);
    process->dir.self =
        namespace_bind(&process->ns, process->dir.dir, &selfPath, MOUNT_OVERWRITE, MODE_DIRECTORY | MODE_ALL_PERMS);
    path_put(&selfPath);
    if (process->dir.self == NULL)
    {
        goto error;
    }

    return 0;

error:
    process_dir_deinit(process);
    return ERR;
}

process_t* process_new(priority_t priority, gid_t gid)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    process->id = atomic_fetch_add(&newPid, 1);
    group_entry_init(&process->groupEntry);
    atomic_init(&process->priority, priority);
    memset_s(process->exitStatus.buffer, NOTE_MAX, 0, NOTE_MAX);
    lock_init(&process->exitStatus.lock);

    if (space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
            SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY) == ERR)
    {
        free(process);
        return NULL;
    }

    namespace_init(&process->ns);
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


    process->dir.self = NULL;
    process->dir.dir = NULL;
    list_init(&process->dir.files);
    process->dir.env = NULL;
    list_init(&process->dir.envEntries);
    lock_init(&process->dir.lock);

    process->cmdline = NULL;
    process->cmdlineSize = 0;

    if (group_add(gid, &process->groupEntry) == ERR)
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

    lock_acquire(&process->exitStatus.lock);
    strncpy_s(process->exitStatus.buffer, NOTE_MAX, status, NOTE_MAX - 1);
    process->exitStatus.buffer[NOTE_MAX - 1] = '\0';
    lock_release(&process->exitStatus.lock);

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
    cwd_clear(&process->cwd);
    file_table_close_all(&process->fileTable);
    namespace_clear(&process->ns);
    group_remove(&process->groupEntry);

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

    if (process->dir.self != NULL)
    {
        UNREF(process->dir.self);
        process->dir.self = NULL;
    }

    if (process->dir.dir != NULL)
    {
        UNREF(process->dir.dir);
        process->dir.dir = NULL;
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

    while (!list_is_empty(&process->dir.envEntries))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_first(&process->dir.envEntries), dentry_t, otherEntry));
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

    dentry_t* srcDentry;
    LIST_FOR_EACH(srcDentry, &src->dir.envEntries, otherEntry)
    {
        inode_t* inode = inode_new(srcDentry->inode->superblock, vfs_id_get(), INODE_FILE, &envFileInodeOps, &envFileOps);
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

        dentry_t* dentry = dentry_new(srcDentry->inode->superblock, dest->dir.env, srcDentry->name);
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
        return 0;
    }

    uint64_t totalSize = 0;
    for (uint64_t i = 0; i < argc; i++)
    {
        if (argv[i] == NULL)
        {
            break;
        }
        totalSize += strlen(argv[i]) + 1;
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
    for (uint64_t i = 0; i < argc; i++)
    {
        if (argv[i] == NULL)
        {
            break;
        }
        uint64_t len = strlen(argv[i]) + 1;
        memcpy(dest, argv[i], len);
        dest += len;
    }

    if (process->cmdline != NULL)
    {
        free(process->cmdline);
    }

    process->cmdline = cmdline;
    process->cmdlineSize = totalSize;

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
        kernelProcess = process_new(PRIORITY_MAX, GID_NONE);
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
    procMount = sysfs_mount_new(NULL, "proc", NULL, MOUNT_PROPAGATE_CHILDREN | MOUNT_PROPAGATE_PARENT,
        MODE_DIRECTORY | MODE_ALL_PERMS, NULL);
    if (procMount == NULL)
    {
        panic(NULL, "Failed to mount /proc dentriesystem");
    }

    selfDir = sysfs_dir_new(procMount->source, "self", NULL, NULL);
    if (selfDir == NULL)
    {
        panic(NULL, "Failed to create /proc/self directory");
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
