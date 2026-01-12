#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/proc/reaper.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <assert.h>
#include <errno.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>
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

static map_t pidMap = MAP_CREATE();
static list_t processes = LIST_CREATE(processes);
static rwlock_t processesLock = RWLOCK_CREATE();

static void process_free(process_t* process)
{
    LOG_DEBUG("freeing process pid=%d\n", process->id);

    assert(list_is_empty(&process->threads.list));

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
    if (process->nspace != NULL)
    {
        UNREF(process->nspace);
    }
    space_deinit(&process->space);
    wait_queue_deinit(&process->dyingQueue);
    wait_queue_deinit(&process->suspendQueue);
    futex_ctx_deinit(&process->futexCtx);
    env_deinit(&process->env);
    free(process);
}

process_t* process_new(priority_t priority, group_member_t* group, namespace_t* ns)
{
    if (ns == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    process_t* process = malloc(sizeof(process_t));
    if (process == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&process->ref, process_free);
    list_entry_init(&process->entry);
    map_entry_init(&process->mapEntry);
    list_entry_init(&process->zombieEntry);
    process->id = atomic_fetch_add(&newPid, 1);
    atomic_init(&process->priority, priority);
    memset_s(process->status.buffer, PROCESS_STATUS_MAX, 0, PROCESS_STATUS_MAX);
    lock_init(&process->status.lock);

    if (space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
            SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY) == ERR)
    {
        free(process);
        return NULL;
    }

    process->nspace = REF(ns);
    lock_init(&process->nspaceLock);
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
    env_init(&process->env);
    process->argv = NULL;
    process->argc = 0;

    if (group_member_init(&process->group, group) == ERR)
    {
        process_free(process);
        return NULL;
    }

    RWLOCK_WRITE_SCOPE(&processesLock);

    map_key_t mapKey = map_key_uint64(process->id);
    if (map_insert(&pidMap, &mapKey, &process->mapEntry) == ERR)
    {
        process_free(process);
        return NULL;
    }

    LOG_DEBUG("created process pid=%d\n", process->id);

    list_push_back(&processes, &process->entry);
    return REF(process);
}

process_t* process_get(pid_t id)
{
    RWLOCK_READ_SCOPE(&processesLock);

    map_key_t mapKey = map_key_uint64(id);
    map_entry_t* entry = map_get(&pidMap, &mapKey);
    if (entry == NULL)
    {
        return NULL;
    }

    return REF(CONTAINER_OF(entry, process_t, mapEntry));
}

namespace_t* process_get_ns(process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    lock_acquire(&process->nspaceLock);
    namespace_t* ns = process->nspace != NULL ? REF(process->nspace) : NULL;
    lock_release(&process->nspaceLock);

    if (ns == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    return ns;
}

void process_set_ns(process_t* process, namespace_t* ns)
{
    if (process == NULL || ns == NULL)
    {
        return;
    }

    lock_acquire(&process->nspaceLock);
    UNREF(process->nspace);
    process->nspace = REF(ns);
    lock_release(&process->nspaceLock);
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
    strncpy(process->status.buffer, status, PROCESS_STATUS_MAX - 1);
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

    cwd_clear(&process->cwd);
    file_table_close_all(&process->fileTable);

    lock_acquire(&process->nspaceLock);
    UNREF(process->nspace);
    process->nspace = NULL;
    lock_release(&process->nspaceLock);

    group_remove(&process->group);

    wait_unblock(&process->dyingQueue, WAIT_ALL, EOK);

    reaper_push(process);
}

void process_remove(process_t* process)
{
    rwlock_write_acquire(&processesLock);
    map_remove(&pidMap, &process->mapEntry);
    list_remove(&process->entry);
    rwlock_write_release(&processesLock);

    UNREF(process);
}

process_t* process_iterate_begin(void)
{
    RWLOCK_READ_SCOPE(&processesLock);

    if (list_is_empty(&processes))
    {
        return NULL;
    }

    return REF(CONTAINER_OF(list_first(&processes), process_t, entry));
}

process_t* process_iterate_next(process_t* prev)
{
    RWLOCK_READ_SCOPE(&processesLock);

    list_entry_t* nextEntry = list_next(&processes, &prev->entry);
    UNREF(prev);

    if (nextEntry == NULL)
    {
        return NULL;
    }

    return REF(CONTAINER_OF(nextEntry, process_t, entry));
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
        namespace_t* ns = namespace_new(NULL);
        if (ns == NULL)
        {
            panic(NULL, "Failed to create kernel namespace");
        }
        UNREF_DEFER(ns);

        kernelProcess = process_new(PRIORITY_MAX, NULL, ns);
        if (kernelProcess == NULL)
        {
            panic(NULL, "Failed to create kernel process");
        }
        LOG_INFO("kernel process initialized with pid=%d\n", kernelProcess->id);
    }

    return kernelProcess;
}

SYSCALL_DEFINE(SYS_GETPID, pid_t)
{
    return sched_process()->id;
}
