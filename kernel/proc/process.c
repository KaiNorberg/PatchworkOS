#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/io/io.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/job.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/ref.h>

#include <assert.h>
#include <libstd/fs.h>
#include <libstd/list.h>
#include <libstd/map.h>
#include <libstd/math.h>
#include <libstd/proc.h>
#include <libstd/status.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static process_t* kernelProcess = NULL;

static _Atomic(proc_t) newPid = ATOMIC_VAR_INIT(0);

static bool pid_map_cmp(map_entry_t* entry, const void* key)
{
    process_t* process = CONTAINER_OF(entry, process_t, mapEntry);
    return process->id == (proc_t)(uintptr_t)key;
}

static MAP_CREATE(pidMap, 64, pid_map_cmp);

list_t _processes = LIST_CREATE(_processes);
static lock_t processesLock = LOCK_CREATE();

static void process_ctor(void* ptr)
{
    process_t* process = (process_t*)ptr;

    process->ref = (ref_t){0};
    list_entry_init(&process->entry);
    map_entry_init(&process->mapEntry);
    list_entry_init(&process->zombieEntry);
    process->id = 0;
    atomic_init(&process->priority, 0);
    memset_s(process->result.buffer, PROCESS_RESULT_MAX, 0, PROCESS_RESULT_MAX);
    lock_init(&process->result.lock);
    process->space = (space_t){0};
    process->files = (file_table_t){0};
    process->perf = (perf_process_ctx_t){0};
    process->noteHandler = (note_handler_t){0};
    process->dyingIrps = (list_t)LIST_CREATE(process->dyingIrps);
    process->dyingIrpsLock = (lock_t)LOCK_CREATE();
    atomic_init(&process->flags, PROCESS_NONE);
    atomic_init(&process->threads.newTid, 0);
    list_init(&process->threads.list);
    process->threads.count = 0;
    lock_init(&process->threads.lock);
    process->start = 0;
    lock_init(&process->args.lock);
    process->args.buffer = NULL;
    process->args.length = 0;
    process->job = (job_member_t){0};
    process->rcu = (rcu_entry_t){0};
}

static cache_t cache = CACHE_CREATE(cache, "process", sizeof(process_t), CACHE_LINE, process_ctor, NULL);

static void process_free(process_t* process)
{
    LOG_DEBUG("freeing process pid=%d\n", process->id);

    assert(list_is_empty(&process->threads.list));

    lock_acquire(&processesLock);
    map_remove(&pidMap, &process->mapEntry, hash_uint64(process->id));
    list_remove_rcu(&process->entry);
    lock_release(&processesLock);

    if (process->args.buffer != NULL)
    {
        free(process->args.buffer);
        process->args.buffer = NULL;
        process->args.length = 0;
    }

    job_member_deinit(&process->job);
    file_table_deinit(&process->files);
    space_deinit(&process->space);
    for (uint64_t i = 0; i < ARRAY_SIZE(process->rings); i++)
    {
        ioring_ctx_deinit(&process->rings[i]);
    }

    rcu_call(&process->rcu, rcu_call_cache_free, process);
}

status_t process_new(process_t** out, prio_t priority, job_t* job)
{
    if (out == NULL || job == NULL)
    {
        return ERR(PROC, INVAL);
    }

    process_t* process = cache_alloc(&cache);
    if (process == NULL)
    {
        return ERR(PROC, NOMEM);
    }

    ref_init(&process->ref, process_free);
    process->id = atomic_fetch_add_explicit(&newPid, 1, memory_order_relaxed);
    atomic_store(&process->priority, priority);
    process->result.buffer[0] = '\0';

    status_t status = space_init(&process->space, VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX,
        SPACE_MAP_KERNEL_BINARY | SPACE_MAP_KERNEL_HEAP | SPACE_MAP_IDENTITY);
    if (IS_ERR(status))
    {
        cache_free(process);
        return status;
    }

    file_table_init(&process->files);
    perf_process_ctx_init(&process->perf);
    for (uint64_t i = 0; i < ARRAY_SIZE(process->rings); i++)
    {
        ioring_ctx_init(&process->rings[i]);
    }
    note_handler_init(&process->noteHandler);
    atomic_store(&process->flags, PROCESS_NONE);
    atomic_store(&process->threads.newTid, 0);
    process->start = clock_uptime();

    job_member_init(&process->job);
    job_join(job, &process->job);

    lock_acquire(&processesLock);
    map_insert(&pidMap, &process->mapEntry, hash_uint64(process->id));
    list_push_back_rcu(&_processes, &process->entry);
    lock_release(&processesLock);

    LOG_DEBUG("created process pid=%d\n", process->id);

    *out = process;
    return OK;
}

process_t* process_get(proc_t id)
{
    lock_acquire(&processesLock);
    map_entry_t* entry = map_find(&pidMap, (void*)(uintptr_t)id, hash_uint64(id));
    if (entry == NULL)
    {
        lock_release(&processesLock);
        return NULL;
    }

    process_t* process = REF_TRY(CONTAINER_OF(entry, process_t, mapEntry));
    lock_release(&processesLock);
    return process;
}

void process_kill(process_t* process, const char* result)
{
    if (atomic_fetch_or(&process->flags, PROCESS_DYING) & PROCESS_DYING)
    {
        return;
    }

    LOG_DEBUG("killing process pid=%d result='%s' ref=%llu\n", process->id, result, atomic_load(&process->ref.count));

    lock_acquire(&process->result.lock);
    strncpy(process->result.buffer, result, PROCESS_RESULT_MAX - 1);
    process->result.buffer[PROCESS_RESULT_MAX - 1] = '\0';
    lock_release(&process->result.lock);

    uint64_t killCount = 0;
    {
        RCU_READ_SCOPE();
        thread_t* thread;
        PROCESS_RCU_THREAD_FOR_EACH(thread, process)
        {
            thread_send_note(thread, "kill");
            killCount++;
        }
    }

    if (killCount > 0)
    {
        LOG_DEBUG("sent kill note to %llu threads in process pid=%d\n", killCount, process->id);
    }

    file_table_drop_all(&process->files);
    job_leave(&process->job);

    lock_acquire(&process->dyingIrpsLock);
    list_t dyingIrps = LIST_CREATE(dyingIrps);
    irp_claim_list(&dyingIrps, &process->dyingIrps);
    lock_release(&process->dyingIrpsLock);

    while (!list_is_empty(&dyingIrps))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&dyingIrps), irp_t, entry);
        irp_frame_t* frame = irp_current(irp);
        switch (frame->major)
        {
        case IRP_MJ_READ:
            lock_acquire(&process->result.lock);
            status_t status = irp_read_helper(irp, process->result.buffer, strlen(process->result.buffer));
            lock_release(&process->result.lock);
            irp_complete(irp, status);
            break;
        case IRP_MJ_POLL:
            irp->result = IOEVENT_READ;
            irp_complete(irp, OK);
            break;
        default:
            irp_complete(irp, ERR(FS, IMPL));
            break;
        }
    }
}

status_t process_send_note(process_t* process, const char* note)
{
    if (process == NULL || note == NULL)
    {
        return ERR(PROC, INVAL);
    }

    RCU_READ_SCOPE();

    thread_t* thread;
    PROCESS_RCU_THREAD_FOR_EACH(thread, process)
    {
        status_t status = thread_send_note(thread, note);
        if (!IS_ERR(status))
        {
            return OK;
        }
    }

    return ERR(PROC, DYING);
}

bool process_has_thread(process_t* process, thrd_t tid)
{
    RCU_READ_SCOPE();

    thread_t* thread;
    PROCESS_RCU_THREAD_FOR_EACH(thread, process)
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
        job_t* job;
        status_t status = job_new(&job, NULL);
        if (IS_ERR(status))
        {
            panic(NULL, "Failed to create kernel job %Y", status);
        }
        UNREF_DEFER(job);

        status = process_new(&kernelProcess, PRIO_MAX, job);
        if (IS_ERR(status))
        {
            panic(NULL, "Failed to create kernel process %Y", status);
        }
        LOG_INFO("kernel process initialized with pid=%d\n", kernelProcess->id);
    }

    return kernelProcess;
}

SYSCALL_DEFINE(SYS_PROC_CURRENT)
{
    *_result = process_current()->id;
    return OK;
}
