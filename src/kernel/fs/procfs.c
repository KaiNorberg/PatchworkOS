#include <kernel/fs/binding.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/status.h>

static dentry_t* root = NULL;

static status_t procfs_prio_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    prio_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, prioStr, length);
}

static status_t procfs_prio_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    char prioStr[MAX_NAME];
    size_t bytesWritten;
    status_t status = mdl_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, prioStr, MAX_NAME - 1);
    if (IS_ERR(status))
    {
        return status;
    }
    prioStr[bytesWritten] = '\0';

    long long int prio = atoll(prioStr);
    if (prio < 0)
    {
        return ERR(FS, INVAL);
    }
    if (prio > PRIO_MAX_USER)
    {
        return ERR(FS, ACCESS);
    }

    atomic_store(&process->priority, prio);
    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t prioClass = {
    .name = "procfs prio",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_prio_read,
            [IRP_MJ_WRITE] = procfs_prio_write,
        },
};

static status_t procfs_cmdline_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    if (process->args == NULL || process->argsLen == 0)
    {
        irp->result = 0;
        return OK;
    }

    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, process->args, process->argsLen);
}

static vnode_class_t cmdlineClass = {
    .name = "procfs cmdline",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_cmdline_read,
        },
};

static status_t procfs_note_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    size_t count = mdl_size(frame->write.buffer);
    if (count == 0)
    {
        irp->result = 0;
        return OK;
    }

    if (count >= NOTE_MAX)
    {
        return ERR(FS, INVAL);
    }

    char string[NOTE_MAX];
    size_t bytesWritten;
    status_t status = mdl_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, string, NOTE_MAX - 1);
    if (IS_ERR(status))
    {
        return status;
    }
    string[bytesWritten] = '\0';

    job_t* job = job_get(&process->job);
    if (job == NULL)
    {
        return ERR(FS, DYING);
    }
    UNREF_DEFER(job);

    if (job_is_leader(job, &process->job))
    {
        job_send_note(job, string);
    }
    else
    {
        process_send_note(process, string);
    }

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t noteClass = {
    .name = "procfs note",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = procfs_note_write,
        },
};

static status_t procfs_pid_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    char pidStr[MAX_NAME];
    uint32_t length = snprintf(pidStr, MAX_NAME, "%llu", process->id);
    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, pidStr, length);
}

static vnode_class_t pidClass = {.name = "procfs pid",
    .type = FILE_TYPE_SYSTEM,
    .handlers = {
        VNODE_HANDLERS(),
        [IRP_MJ_READ] = procfs_pid_read,
    }};

static status_t procfs_wait_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    lock_acquire(&process->dyingIrpsLock);
    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }
    lock_release(&process->dyingIrpsLock);

    return OK;
}

static status_t procfs_wait_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    if (!(atomic_load(&process->flags) & PROCESS_DYING))
    {
        LOCK_SCOPE(&process->dyingIrpsLock);
        return irp_delay(irp, &process->dyingIrps, procfs_wait_cancel);
    }

    lock_acquire(&process->result.lock);
    status_t status = mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, process->result.buffer,
        strlen(process->result.buffer));
    lock_release(&process->result.lock);
    return status;
}

static status_t procfs_wait_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    if (!(atomic_load(&process->flags) & PROCESS_DYING))
    {
        LOCK_SCOPE(&process->dyingIrpsLock);
        return irp_delay(irp, &process->dyingIrps, procfs_wait_cancel);
    }

    irp->result = IOEVENT_READ;
    return OK;
}

static vnode_class_t waitClass = {.name = "procfs wait",
    .type = FILE_TYPE_SYSTEM,
    .handlers = {
        VNODE_HANDLERS(),
        [IRP_MJ_READ] = procfs_wait_read,
        [IRP_MJ_POLL] = procfs_wait_poll,
    }};

static status_t procfs_perf_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    size_t userPages = space_user_page_count(&process->space);

    lock_acquire(&process->threads.lock);
    size_t threadCount = process->threads.count;
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
        return ERR(FS, IMPL);
    }

    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, statStr, length);
}

static vnode_class_t perfClass = {
    .name = "procfs perf",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_perf_read,
        },
};

static status_t procfs_ctl_control(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    iocmd_t cmd = frame->control.command;
    const char* args = frame->control.args;

    switch (cmd)
    {
    case IOCMD('c', 'l', 'o', 's', 'e'):
    {
        fd_t fd1;
        fd_t fd2;
        int count = sscanf(args, "%lld %lld", &fd1, &fd2);
        if (count == 1)
        {
            return file_table_drop(&process->files, fd1);
        }
        if (count == 2)
        {
            file_table_drop_range(&process->files, fd1, fd2);
            return OK;
        }
        return ERR(FS, INVAL);
    }
    case IOCMD('d', 'u', 'p'):
    {
        fd_t oldFd;
        fd_t newFd;
        if (sscanf(args, "%lld %lld", &oldFd, &newFd) != 2)
        {
            return ERR(FS, INVAL);
        }
        return file_table_dup(&process->files, oldFd, &newFd);
    }
    case IOCMD('s', 't', 'a', 'r', 't'):
    {
        atomic_fetch_and(&process->flags, ~PROCESS_SUSPENDED);
        wait_unblock(&process->suspendQueue, WAIT_ALL, OK);
        return OK;
    }
    case IOCMD('k', 'i', 'l', 'l'):
    {
        if (args[0] == '\0')
        {
            process_kill(process, "killed");
            return OK;
        }
        process_kill(process, args);
        return OK;
    }
    default:
        return ERR(FS, INVAL_CTL);
    }
}

static vnode_class_t ctlClass = {
    .name = "procfs ctl",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = ctl_generic_write,
            [IRP_MJ_CONTROL] = procfs_ctl_control,
        },
};

typedef struct
{
    const char* name;
    vnode_class_t* cls;
} procfs_entry_t;

static const procfs_entry_t dirEntries[] = {
    {"prio", &prioClass},
    {"cmdline", &cmdlineClass},
    {"note", &noteClass},
    {"pid", &pidClass},
    {"wait", &waitClass},
    {"perf", &perfClass},
    {"ctl", &ctlClass},
};

static status_t procfs_dir_lookup(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    assert(process != NULL);

    dentry_t* target = frame->lookup.dentry;

    for (size_t i = 0; i < ARRAY_SIZE(dirEntries); i++)
    {
        if (strcmp(target->name, dirEntries[i].name) != 0)
        {
            continue;
        }

        file_number_t number = (i << 1) | (process->id << 8);
        vnode_t* vnode = vnode_new(frame->vnode->volume, dirEntries[i].cls, number);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = process; // No reference

        dentry_make_positive(target, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t procfs_dir_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    assert(process != NULL);

    diremit_t emit;
    diremit_begin(&emit, irp);

    for (size_t i = 0; i < ARRAY_SIZE(dirEntries); i++)
    {
        if (!diremit(&emit, dirEntries[i].name))
        {
            break;
        }
    }

    return OK;
}

static status_t procfs_dir_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    assert(process != NULL);

    UNREF(process);
    frame->vnode->data = NULL;
    return OK;
}

static bool procfs_dir_access(dentry_t* dentry)
{
    process_t* current = process_current();
    assert(current != NULL);
    process_t* process = dentry->vnode->data;
    assert(process != NULL);

    job_t* currentJob = job_get(&current->job);
    UNREF_DEFER(currentJob);

    job_t* processJob = job_get(&process->job);
    UNREF_DEFER(processJob);

    return job_is_accessible(currentJob, processJob);
}

static vnode_class_t dirClass = {
    .name = "procfs dir",
    .type = FILE_TYPE_DIRECTORY,
    .access = procfs_dir_access,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_LOOKUP] = procfs_dir_lookup,
            [IRP_MJ_READ] = procfs_dir_read,
            [IRP_MJ_RECLAIM] = procfs_dir_reclaim,
        },
};

static status_t procfs_self_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    char pidStr[MAX_NAME];
    int ret = snprintf(pidStr, ARRAY_SIZE(pidStr), "%llu", irp->process->id);
    if (ret < 0)
    {
        return ERR(FS, IMPL);
    }

    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, pidStr, ret);
}

static vnode_class_t selfClass = {
    .name = "procfs self",
    .type = FILE_TYPE_SYMLINK,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_self_read,
        },
};

static const procfs_entry_t rootEntries[] = {
    {"self", &selfClass},
};

static status_t procfs_root_lookup(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    assert(process != NULL);

    dentry_t* target = frame->lookup.dentry;

    for (size_t i = 0; i < ARRAY_SIZE(rootEntries); i++)
    {
        if (strcmp(target->name, rootEntries[i].name) != 0)
        {
            continue;
        }

        file_number_t number = ((i << 1) | (process->id << 8)) + 1;
        vnode_t* vnode = vnode_new(frame->vnode->volume, rootEntries[i].cls, number);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);

        dentry_make_positive(target, vnode);
        return OK;
    }

    proc_t pid;
    if (sscanf(target->name, "%llu", &pid) != 1)
    {
        return INFO(FS, NEGATIVE);
    }

    process_t* check = process_get(pid);
    if (check == NULL)
    {
        return INFO(FS, NEGATIVE);
    }
    UNREF_DEFER(check);

    file_number_t number = pid;
    vnode_t* vnode = vnode_new(frame->vnode->volume, &dirClass, number);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = REF(process);

    dentry_make_positive(target, vnode);
    return OK;
}

static status_t procfs_root_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    diremit_t emit;
    diremit_begin(&emit, irp);

    for (size_t i = 0; i < ARRAY_SIZE(rootEntries); i++)
    {
        if (!diremit(&emit, rootEntries[i].name))
        {
            break;
        }
    }

    RCU_READ_SCOPE();

    process_t* process;
    PROCESS_RCU_FOR_EACH(process)
    {
        char name[MAX_NAME];
        snprintf(name, sizeof(name), "%llu", process->id);
        if (!diremit(&emit, name))
        {
            break;
        }
    }

    return OK;
}

static vnode_class_t rootClass = {
    .name = "procfs root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_LOOKUP] = procfs_root_lookup,
            [IRP_MJ_READ] = procfs_root_read,
        },
};

static status_t procfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    return file_redirect(file, root);
}

static vnode_class_t cloneClass = {
    .name = "procfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = procfs_clone_open,
        },
};

static filesystem_t procfs = {
    .name = PROCFS_NAME,
    .clone = &cloneClass,
};

void procfs_init(void)
{
    vnode_t* vnode = vnode_new(volume_new(), &dirClass, 0);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create procfs root vnode");
    }
    UNREF_DEFER(vnode);

    root = dentry_new(NULL, NULL);
    if (root == NULL)
    {
        panic(NULL, "Failed to create procfs root dentry");
    }

    dentry_make_positive(root, vnode);

    status_t status = filesystem_register(&procfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register procfs filesystem %Y", status);
    }
}