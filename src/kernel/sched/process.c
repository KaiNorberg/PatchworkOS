#include <errno.h>
#include <kernel/sched/process.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <assert.h>
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

static mount_t* procMount = NULL;
static dentry_t* selfDir = NULL;

static list_t zombies = LIST_CREATE(zombies);
static clock_t nextReaperTime = CLOCKS_NEVER;
static lock_t zombiesLock = LOCK_CREATE();

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

static file_ops_t cwdOps = {
    .read = process_cwd_read,
};

static uint64_t process_cmdline_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)buffer; // Unused
    (void)count;  // Unused
    (void)offset; // Unused

    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    return 0;

    // uint64_t length;
    // const char* strings = strv_get_strings(&process->argv, &length);
    // return BUFFER_READ(buffer, count, offset, strings, length);
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

    // Special case, kill should kill all threads in the process
    if (count == 4 && memcmp(buffer, "kill", 4) == 0)
    {
        process_kill(process, 0);
        return count;
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

static uint64_t process_wait_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return ERR;
    }

    WAIT_BLOCK(&process->dyingWaitQueue, atomic_load(&process->isDying));

    char status[MAX_PATH];
    int length = snprintf(status, sizeof(status), "%lld", atomic_load(&process->status));
    if (length < 0)
    {
        return ERR;
    }

    return BUFFER_READ(buffer, count, offset, status, (uint64_t)length);
}

static wait_queue_t* process_wait_poll(file_t* file, poll_events_t* revents)
{
    process_t* process = process_file_get_process(file);
    if (process == NULL)
    {
        return NULL;
    }

    if (atomic_load(&process->isDying))
    {
        *revents |= POLLIN;
        return &process->dyingWaitQueue;
    }

    return &process->dyingWaitQueue;
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
        "user_clocks %llu\nkernel_clocks %llu\nstart_clocks %llu\nuser_pages %llu\nthread_count %llu", userClocks,
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

static inode_ops_t envInodeOps = {
    .create = process_env_create,
    .remove = process_env_remove,
    .cleanup = process_env_cleanup,
};

static uint64_t process_env_create(inode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = dir->private;
    assert(process != NULL);

    inode_t* inode = inode_new(dir->superblock, vfs_get_new_id(), INODE_FILE, NULL, &envFileOps);
    if (inode == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(inode);

    inode->private = NULL;
    inode->size = 0;

    dentry_make_positive(target, inode);

    lock_acquire(&process->dentriesLock);
    list_push_back(&process->envVars, &target->otherEntry);
    REF(target);
    lock_release(&process->dentriesLock);

    return 0;
}

static uint64_t process_env_remove(inode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        errno = EINVAL;
        return ERR;
    }

    inode_t* inode = dentry_inode_get(target);
    if (inode == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DEREF_DEFER(inode);

    MUTEX_SCOPE(&inode->mutex);

    process_t* process = dir->private;
    assert(process != NULL);

    lock_acquire(&process->dentriesLock);
    list_remove(&process->envVars, &target->otherEntry);
    DEREF(target);
    lock_release(&process->dentriesLock);

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
    DEREF(process);
}

static inode_ops_t procInodeOps = {
    .cleanup = process_proc_cleanup,
};

static void process_free(process_t* process)
{
    LOG_DEBUG("freeing process pid=%d\n", process->id);
    assert(list_is_empty(&process->threads.list));

    while (!list_is_empty(&process->dentries))
    {
        dentry_t* dentry = CONTAINER_OF_SAFE(list_pop_first(&process->dentries), dentry_t, otherEntry);
        DEREF(dentry);
    }

    if (list_length(&process->threads.list) != 0)
    {
        panic(NULL, "Attempt to free process pid=%llu with present threads\n", process->id);
    }

    space_deinit(&process->space);
    wait_queue_deinit(&process->dyingWaitQueue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static uint64_t process_dir_init(process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!list_is_empty(&process->dentries))
    {
        return 0;
    }

    char name[MAX_NAME];
    int ret = snprintf(name, MAX_NAME, "%d", process->id);
    if (ret < 0 || ret >= MAX_NAME)
    {
        return ERR;
    }

    process->proc = sysfs_dir_new(procMount->root, name, &procInodeOps, REF(process));
    if (process->proc == NULL)
    {
        return ERR;
    }
    list_push_back(&process->dentries, &process->proc->otherEntry);

    process->env = sysfs_dir_new(process->proc, "env", &envInodeOps, REF(process));
    if (process->env == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &process->env->otherEntry);

    dentry_t* prio = sysfs_file_new(process->proc, "prio", NULL, &prioOps, REF(process));
    if (prio == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &prio->otherEntry);

    dentry_t* cwd = sysfs_file_new(process->proc, "cwd", NULL, &cwdOps, REF(process));
    if (cwd == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &cwd->otherEntry);

    dentry_t* cmdline = sysfs_file_new(process->proc, "cmdline", NULL, &cmdlineOps, REF(process));
    if (cmdline == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &cmdline->otherEntry);

    dentry_t* note = sysfs_file_new(process->proc, "note", NULL, &noteOps, REF(process));
    if (note == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &note->otherEntry);

    dentry_t* wait = sysfs_file_new(process->proc, "wait", NULL, &waitOps, REF(process));
    if (wait == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &wait->otherEntry);

    dentry_t* perf = sysfs_file_new(process->proc, "perf", NULL, &statOps, REF(process));
    if (perf == NULL)
    {
        goto error;
    }
    list_push_back(&process->dentries, &perf->otherEntry);

    path_t selfPath = PATH_CREATE(procMount, selfDir);
    process->self = namespace_bind(&process->ns, process->proc, &selfPath, MOUNT_OVERWRITE | MOUNT_PROPAGATE_CHILDREN,
        MODE_ALL_PERMS);
    path_put(&selfPath);
    if (process->self == NULL)
    {
        goto error;
    }

    return 0;

error:
    while (!list_is_empty(&process->dentries))
    {
        dentry_t* dentry = CONTAINER_OF_SAFE(list_pop_first(&process->dentries), dentry_t, otherEntry);
        DEREF(dentry);
    }
    return ERR;
}

process_t* process_new(priority_t priority)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&process->ref, process_free);
    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    atomic_init(&process->status, EXIT_SUCCESS);

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
    wait_queue_init(&process->dyingWaitQueue);
    atomic_init(&process->isDying, false);

    process->threads.newTid = 0;
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

    list_entry_init(&process->zombieEntry);

    list_init(&process->dentries);
    list_init(&process->envVars);
    lock_init(&process->dentriesLock);
    process->self = NULL;

    if (process->id != 0) // Delay kernel process /proc dir init
    {
        if (process_dir_init(process) == ERR)
        {
            process_free(process);
            return NULL;
        }
    }

    return process;
}

void process_kill(process_t* process, int32_t status)
{
    LOG_DEBUG("killing process pid=%d with status=%lld refCount=%d\n", process->id, status, process->ref.count);
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

    // Anything that another process could be waiting on must be cleaned up here.
    cwd_deinit(&process->cwd);
    file_table_deinit(&process->fileTable);
    namespace_deinit(&process->ns);
    // The dir entries have refs to the process, but a parent process might want to read files in /proc/[pid] after the
    // process has exited especially its wait file, so we defer dereferencing them until the reaper runs.
    wait_unblock(&process->dyingWaitQueue, WAIT_ALL, EOK);

    LOCK_SCOPE(&zombiesLock);
    list_push_back(&zombies, &REF(process)->zombieEntry);
    nextReaperTime = sys_time_uptime() + CONFIG_PROCESS_REAPER_INTERVAL; // Delay reaper run
}

uint64_t process_copy_env(process_t* dest, process_t* src)
{
    if (dest == NULL || src == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return 0;

    LOCK_SCOPE(&src->dentriesLock);
    LOCK_SCOPE(&dest->dentriesLock);

    if (!list_is_empty(&dest->envVars))
    {
        errno = EBUSY;
        return ERR;
    }

    dentry_t* srcDentry;
    LIST_FOR_EACH(srcDentry, &src->envVars, otherEntry)
    {
        inode_t* inode = inode_new(srcDentry->inode->superblock, vfs_get_new_id(), INODE_FILE, NULL, &envFileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(inode);

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

        dentry_t* dentry = dentry_new(srcDentry->inode->superblock, dest->env, srcDentry->name);
        if (dentry == NULL)
        {
            return ERR;
        }

        dentry_make_positive(dentry, inode);
        list_push_back(&dest->envVars, &dentry->otherEntry);
    }

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
        kernelProcess = process_new(PRIORITY_MAX);
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
    procMount =
        sysfs_mount_new(NULL, "proc", NULL, MOUNT_PROPAGATE_CHILDREN | MOUNT_PROPAGATE_PARENT, MODE_ALL_PERMS, NULL);
    if (procMount == NULL)
    {
        panic(NULL, "Failed to mount /proc filesystem");
    }

    selfDir = sysfs_dir_new(procMount->root, "self", NULL, NULL);
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

static void process_reaper(void* arg)
{
    (void)arg;

    while (1)
    {
        sched_nanosleep(CONFIG_PROCESS_REAPER_INTERVAL);

        lock_acquire(&zombiesLock);
        clock_t uptime = sys_time_uptime();
        if (uptime < nextReaperTime)
        {
            lock_release(&zombiesLock);
            continue;
        }
        nextReaperTime = CLOCKS_NEVER;

        list_t localZombies = LIST_CREATE(localZombies);

        while (!list_is_empty(&zombies))
        {
            process_t* process = CONTAINER_OF(list_pop_first(&zombies), process_t, zombieEntry);
            list_push_back(&localZombies, &process->zombieEntry);
        }

        lock_release(&zombiesLock);

        while (!list_is_empty(&localZombies))
        {
            process_t* process = CONTAINER_OF(list_pop_first(&localZombies), process_t, zombieEntry);

            while (!list_is_empty(&process->dentries))
            {
                dentry_t* dentry = CONTAINER_OF_SAFE(list_pop_first(&process->dentries), dentry_t, otherEntry);
                DEREF(dentry);
            }

            while (!list_is_empty(&process->envVars))
            {
                dentry_t* dentry = CONTAINER_OF_SAFE(list_pop_first(&process->envVars), dentry_t, otherEntry);
                DEREF(dentry);
            }

            DEREF(process->self);
            process->self = NULL;

            DEREF(process);
        }
    }
}

void process_reaper_init(void)
{
    if (thread_kernel_create(process_reaper, NULL) == ERR)
    {
        panic(NULL, "Failed to create process reaper thread");
    }
}

SYSCALL_DEFINE(SYS_GETPID, pid_t)
{
    return sched_process()->id;
}
