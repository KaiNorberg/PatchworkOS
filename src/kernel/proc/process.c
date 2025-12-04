#include <kernel/proc/process.h>

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

static process_t kernelProcess;
static bool kernelProcessInitalized = false;

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
    int length = snprintf(status, sizeof(status), "%llu", atomic_load(&process->status));
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
        "user_clocks %lu\nkernel_clocks %lu\nstart_clocks %lu\nuser_pages %lu\nthread_count %lu", userClocks,
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
    process->dir = sysfs_dir_new(procMount->root, name, &inodeOps, REF(process));
    if (process->dir == NULL)
    {
        return ERR;
    }

    process->prioFile = sysfs_file_new(process->dir, "prio", &inodeOps, &prioOps, REF(process));
    if (process->prioFile == NULL)
    {
        goto error;
    }

    process->cwdFile = sysfs_file_new(process->dir, "cwd", &inodeOps, &cwdOps, REF(process));
    if (process->cwdFile == NULL)
    {
        goto error;
    }

    process->cmdlineFile = sysfs_file_new(process->dir, "cmdline", &inodeOps, &cmdlineOps, REF(process));
    if (process->cmdlineFile == NULL)
    {
        goto error;
    }

    process->noteFile = sysfs_file_new(process->dir, "note", &inodeOps, &noteOps, REF(process));
    if (process->noteFile == NULL)
    {
        goto error;
    }

    process->waitFile = sysfs_file_new(process->dir, "wait", &inodeOps, &waitOps, REF(process));
    if (process->waitFile == NULL)
    {
        goto error;
    }

    process->perfFile = sysfs_file_new(process->dir, "perf", &inodeOps, &statOps, REF(process));
    if (process->perfFile == NULL)
    {
        goto error;
    }

    path_t selfPath = PATH_CREATE(procMount, selfDir);
    process->self = namespace_bind(&process->ns, process->dir, &selfPath, MOUNT_OVERWRITE);
    path_put(&selfPath);
    if (process->self == NULL)
    {
        goto error;
    }

    return 0;

error:
    DEREF(process->waitFile);
    DEREF(process->noteFile);
    DEREF(process->cmdlineFile);
    DEREF(process->cwdFile);
    DEREF(process->prioFile);
    DEREF(process->dir);
    return ERR;
}

static void process_free(process_t* process)
{
    LOG_DEBUG("freeing process pid=%d\n", process->id);
    assert(list_is_empty(&process->threads.list));

    if (!atomic_load(&process->isDying))
    {
        process_kill(process, EXIT_SUCCESS);
    }

    space_deinit(&process->space);
    argv_deinit(&process->argv);
    wait_queue_deinit(&process->dyingWaitQueue);
    futex_ctx_deinit(&process->futexCtx);
    free(process);
}

static uint64_t process_init(process_t* process, const char** argv, const path_t* cwd, namespace_t* parentNs,
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

    if (space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
            SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY) == ERR)
    {
        argv_deinit(&process->argv);
        return ERR;
    }

    if (namespace_init(&process->ns, parentNs) == ERR)
    {
        space_deinit(&process->space);
        argv_deinit(&process->argv);
        return ERR;
    }

    cwd_init(&process->cwd, cwd);
    file_table_init(&process->fileTable);
    futex_ctx_init(&process->futexCtx);
    perf_process_ctx_init(&process->perf);
    wait_queue_init(&process->dyingWaitQueue);
    atomic_init(&process->isDying, false);

    process->threads.newTid = 0;
    list_init(&process->threads.list);
    lock_init(&process->threads.lock);

    list_entry_init(&process->zombieEntry);

    process->dir = NULL;
    process->prioFile = NULL;
    process->cwdFile = NULL;
    process->cmdlineFile = NULL;
    process->noteFile = NULL;
    process->waitFile = NULL;
    process->perfFile = NULL;
    process->self = NULL;

    LOG_DEBUG("new pid=%d priority=%d\n", process->id, priority);
    return 0;
}

process_t* process_new(const char** argv, const path_t* cwd, namespace_t* parentNs, priority_t priority)
{
    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        return NULL;
    }

    if (process_init(process, argv, cwd, parentNs, priority) == ERR)
    {
        free(process);
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

    // Anything that another process could be waiting on must be cleaned up here.
    cwd_deinit(&process->cwd);
    file_table_deinit(&process->fileTable);
    namespace_deinit(&process->ns);
    // The dir entries have refs to the process, but a parent process might want to read files in /proc/[pid] after the
    // process has exited especially its wait file, so for now we defer dereferencing them until the reaper runs. This
    // is really not ideal so TODO: implement a proper reaper.
    wait_unblock(&process->dyingWaitQueue, WAIT_ALL, EOK);

    LOCK_SCOPE(&zombiesLock);
    list_push_back(&zombies, &REF(process)->zombieEntry);
    nextReaperTime = sys_time_uptime() + CONFIG_PROCESS_REAPER_INTERVAL; // Delay reaper run
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

void process_procfs_init(void)
{
    procMount = sysfs_mount_new(NULL, "proc", NULL, MOUNT_PROPAGATE_CHILDREN | MOUNT_PROPAGATE_PARENT, NULL);
    if (procMount == NULL)
    {
        panic(NULL, "Failed to mount /proc filesystem");
    }

    selfDir = sysfs_dir_new(procMount->root, "self", NULL, NULL);
    if (selfDir == NULL)
    {
        panic(NULL, "Failed to create /proc/self directory");
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
            DEREF(process->dir);
            DEREF(process->prioFile);
            DEREF(process->cwdFile);
            DEREF(process->cmdlineFile);
            DEREF(process->noteFile);
            DEREF(process->waitFile);
            DEREF(process->perfFile);
            DEREF(process->self);

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
