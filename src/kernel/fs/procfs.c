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

static bool procfs_access_hide(dentry_t* dentry)
{
    process_t* current = process_current();
    assert(current != NULL);
    process_t* process = dentry->vnode->data;
    assert(process != NULL);

    namespace_t* currentNs = process_get_ns(current);
    UNREF_DEFER(currentNs);

    namespace_t* processNs = process_get_ns(process);
    UNREF_DEFER(processNs);

    return namespace_accessible(currentNs, processNs);
}

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
    .access = procfs_access_hide,
    .handlers =
        {
            [IRP_MJ_READ] = procfs_prio_read,
            [IRP_MJ_WRITE] = procfs_prio_write,
        },
};

static status_t procfs_cmdline_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    if (process->argv == NULL || process->argc == 0)
    {
        irp->result = 0;
        return OK;
    }

    size_t totalSize = 0;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        totalSize += strlen(process->argv[i]) + 1;
    }

    if (totalSize == 0)
    {
        irp->result = 0;
        return OK;
    }

    char* cmdline = malloc(totalSize);
    if (cmdline == NULL)
    {
        return ERR(FS, NOMEM);
    }

    char* dest = cmdline;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        uint64_t len = strlen(process->argv[i]) + 1;
        memcpy(dest, process->argv[i], len);
        dest += len;
    }

    status_t status = mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, cmdline, totalSize);
    free(cmdline);
    return status;
}

static vnode_class_t cmdlineClass = {.name = "procfs cmdline",
    .type = FILE_TYPE_SYSTEM,
    .handlers = {
        [IRP_MJ_READ] = procfs_cmdline_read,
    }};

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

    RCU_READ_SCOPE();

    thread_t* thread = process_rcu_first_thread(process);
    if (thread == NULL)
    {
        return ERR(FS, INVAL);
    }

    status = thread_send_note(thread, string);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t noteClass = {.name = "procfs note",
    .type = FILE_TYPE_SYSTEM,
    .access = procfs_access_hide,
    .handlers = {
        [IRP_MJ_WRITE] = procfs_note_write,
    }};

static status_t procfs_notegroup_write(irp_t* irp)
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

    status = group_send_note(&process->group, string);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t notegroupClass = {
    .name = "procfs notegroup",
    .type = FILE_TYPE_SYSTEM,
    .access = procfs_access_hide,
    .handlers =
        {
            [IRP_MJ_WRITE] = procfs_notegroup_write,
        },
};

static status_t procfs_group_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    process_t* process = frame->vnode->data;

    group_t* group = group_get(&process->group);
    if (group == NULL)
    {
        return ERR(FS, NOGROUP);
    }

    file->data = group;
    return OK;
}

static status_t procfs_group_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    group_t* group = file->data;
    if (group == NULL)
    {
        return OK;
    }

    UNREF(group);
    file->data = NULL;
    return OK;
}

static vnode_class_t groupClass = {
    .name = "procfs group",
    .type = FILE_TYPE_SYSTEM,
    .access = procfs_access_hide,
    .handlers =
        {
            [IRP_MJ_OPEN] = procfs_group_open,
            [IRP_MJ_CLOSE] = procfs_group_close,
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
        [IRP_MJ_READ] = procfs_wait_read,
        [IRP_MJ_POLL] = procfs_wait_poll,
    }};

static status_t procfs_perf_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    size_t userPages = space_user_page_count(&process->space);

    RCU_READ_SCOPE();

    size_t threadCount = process_rcu_thread_count(process);

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
            [IRP_MJ_READ] = procfs_perf_read,
        },
};

static status_t procfs_ns_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    process_t* process = frame->vnode->data;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(FS, NOENT);
    }

    file->data = ns;
    return OK;
}

static status_t procfs_ns_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file->data == NULL)
    {
        return OK;
    }

    UNREF(file->data);
    file->data = NULL;
    return OK;
}

static vnode_class_t nsClass = {
    .name = "procfs ns",
    .type = FILE_TYPE_SYSTEM,
    .access = procfs_access_hide,
    .handlers =
        {
            [IRP_MJ_OPEN] = procfs_ns_open,
            [IRP_MJ_CLOSE] = procfs_ns_close,
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
    case IOCMD('s', 'e', 't', 'n', 's'):
    {
        fd_t fd;
        if (sscanf(args, "%lld", &fd) != 1)
        {
            return ERR(FS, INVAL);
        }

        file_t* nsFile = file_table_get(&process->files, fd);
        if (nsFile == NULL)
        {
            return ERR(FS, BADFD);
        }
        UNREF_DEFER(nsFile);

        if (nsFile->path.dentry->vnode->cls != &nsClass)
        {
            return ERR(FS, INVAL);
        }

        namespace_t* ns = nsFile->data;
        if (ns == NULL)
        {
            return ERR(FS, INVAL);
        }

        process_set_ns(process, ns);
        return OK;
    }
    case IOCMD('s', 'e', 't', 'g', 'r', 'o', 'u', 'p'):
    {
        fd_t fd;
        if (sscanf(args, "%lld", &fd) != 1)
        {
            return ERR(FS, INVAL);
        }

        file_t* groupFile = file_table_get(&process->files, fd);
        if (groupFile == NULL)
        {
            return ERR(FS, BADFD);
        }
        UNREF_DEFER(groupFile);

        if (groupFile->path.dentry->vnode->cls != &groupClass)
        {
            return ERR(FS, INVAL);
        }

        group_t* target = groupFile->data;
        if (target == NULL)
        {
            return ERR(FS, INVAL);
        }

        group_add(target, &process->group);
        return OK;
    }
    default:
        return ERR(FS, INVAL_CTL);
    }
}

static vnode_class_t ctlClass = {.name = "procfs ctl",
    .type = FILE_TYPE_SYSTEM,
    .access = procfs_access_hide,
    .handlers = {
        [IRP_MJ_WRITE] = ctl_generic_write,
        [IRP_MJ_CONTROL] = procfs_ctl_control,
    }};

static status_t procfs_env_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    file_t* file = frame->file;

    const char* value = env_get(&process->env, file->path.dentry->name);
    if (value == NULL)
    {
        irp->result = 0;
        return OK;
    }

    size_t length = strlen(value);
    return mdl_copy_in(frame->read.buffer, SIZE_MAX, 0, &irp->result, value, length);
}

static status_t procfs_env_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;
    file_t* file = frame->file;

    char value[MAX_NAME];
    size_t bytesWritten;
    status_t status = mdl_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, value, MAX_NAME - 1);
    if (IS_ERR(status))
    {
        return status;
    }
    value[bytesWritten] = '\0';

    status = env_set(&process->env, file->path.dentry->name, value);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t envFileClass = {.name = "procfs env file",
    .type = FILE_TYPE_SYSTEM,
    .handlers = {
        [IRP_MJ_READ] = procfs_env_read,
        [IRP_MJ_WRITE] = procfs_env_write,
    }};

static status_t procfs_env_lookup(vnode_t* dir, dentry_t* target)
{
    process_t* process = dir->data;
    assert(process != NULL);

    if (env_get(&process->env, target->name) == NULL)
    {
        return INFO(FS, NEGATIVE);
    }

    vnode_t* vnode = vnode_new(dir->volume, &envFileClass);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = process; // No reference

    dentry_make_positive(target, vnode);

    return OK;
}

static status_t procfs_env_create(vnode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        return ERR(FS, INVAL);
    }

    process_t* process = dir->data;
    assert(process != NULL);

    status_t status = env_set(&process->env, target->name, "");
    if (IS_ERR(status))
    {
        return status;
    }

    vnode_t* vnode = vnode_new(dir->volume, &envFileClass);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = process; // No reference

    dentry_make_positive(target, vnode);
    return OK;
}

static status_t procfs_env_remove(vnode_t* dir, dentry_t* target)
{
    process_t* process = dir->data;
    assert(process != NULL);

    return env_unset(&process->env, target->name);
}

static status_t procfs_env_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    process_t* process = dentry->vnode->data;
    assert(process != NULL);

    MUTEX_SCOPE(&process->env.mutex);

    for (size_t i = 0; i < process->env.count; i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, process->env.vars[i].key, FILE_TYPE_SYSTEM))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_class_t envDirClass = {
    .name = "procfs env dir",
    .type = VNODE_DIR,
    .access = procfs_access_hide,
    .lookup = procfs_env_lookup,
    .create = procfs_env_create,
    .remove = procfs_env_remove,
    .iterate = procfs_env_iterate,
};

static status_t procfs_self_readlink(vnode_t* vnode, char* buffer, size_t size, size_t* bytesRead)
{
    UNUSED(vnode);

    process_t* process = process_current();
    int ret = snprintf(buffer, size, "%llu", process->id);
    if (ret < 0)
    {
        return ERR(FS, IMPL);
    }

    if ((size_t)ret >= size)
    {
        return ERR(FS, NAMETOOLONG);
    }

    *bytesRead = ret;
    return OK;
}

static vnode_class_t selfClass = {
    .name = "procfs self",
    .type = VNODE_SYMLINK,
    .readlink = procfs_self_readlink,
};

typedef struct
{
    const char* name;
    vnode_class_t* cls;
} procfs_entry_t;

static const procfs_entry_t pidEntries[] = {
    {"prio", &prioClass},
    {"cwd", &cwdClass},
    {"cmdline", &cmdlineClass},
    {"note", &noteClass},
    {"notegroup", &notegroupClass},
    {"group", &groupClass},
    {"pid", &pidClass},
    {"wait", &waitClass},
    {"perf", &perfClass},
    {"ns", &nsClass},
    {"ctl", &ctlClass},
    {"env", &envDirClass},
};

static const procfs_entry_t procEntries[] = {
    {"self", &selfClass},
};

static status_t procfs_pid_lookup(vnode_t* dir, dentry_t* target)
{
    process_t* process = dir->data;
    assert(process != NULL);

    for (size_t i = 0; i < ARRAY_SIZE(pidEntries); i++)
    {
        if (strcmp(target->name, pidEntries[i].name) != 0)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dir->volume, pidEntries[i].cls);
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

static void procfs_pid_cleanup(vnode_t* vnode)
{
    process_t* process = vnode->data;
    if (process == NULL)
    {
        return;
    }

    UNREF(process);
    vnode->data = NULL;
}

static status_t procfs_pid_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    process_t* current = process_current();
    process_t* process = dentry->vnode->data;
    assert(process != NULL);

    for (size_t i = 0; i < ARRAY_SIZE(pidEntries); i++)
    {
        if (pidEntries[i].cls->access == procfs_access_hide)
        {
            namespace_t* currentNs = process_get_ns(current);
            UNREF_DEFER(currentNs);

            namespace_t* processNs = process_get_ns(process);
            UNREF_DEFER(processNs);

            if (!namespace_accessible(currentNs, processNs))
            {
                continue;
            }
        }

        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, pidEntries[i].name, pidEntries[i].cls->type))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_class_t pidDirClass = {
    .name = "procfs pid dir",
    .type = VNODE_DIR,
    .lookup = procfs_pid_lookup,
    .cleanup = procfs_pid_cleanup,
    .iterate = procfs_pid_iterate,
};

static status_t procfs_lookup(vnode_t* dir, dentry_t* target)
{
    for (size_t i = 0; i < ARRAY_SIZE(procEntries); i++)
    {
        if (strcmp(target->name, procEntries[i].name) != 0)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dir->volume, procEntries[i].cls);
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

    process_t* process = process_get(pid);
    if (process == NULL)
    {
        return INFO(FS, NEGATIVE);
    }
    UNREF_DEFER(process);

    vnode_t* vnode = vnode_new(dir->volume, &pidDirClass);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = REF(process);

    dentry_make_positive(target, vnode);
    return OK;
}

static status_t procfs_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    for (size_t i = 0; i < ARRAY_SIZE(procEntries); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, procEntries[i].name, procEntries[i].cls->type))
        {
            return OK;
        }
    }

    RCU_READ_SCOPE();

    process_t* process;
    PROCESS_RCU_FOR_EACH(process)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        char name[MAX_NAME];
        snprintf(name, sizeof(name), "%llu", process->id);
        if (!ctx->emit(ctx, name, VNODE_DIR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_class_t rootClass = {
    .name = "procfs root",
    .type = VNODE_DIR,
    .lookup = procfs_lookup,
    .iterate = procfs_iterate,
};

static status_t procfs_mount(filesystem_t* fs, dentry_t** out, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        return ERR(FS, INVAL);
    }

    volume_t* volume = volume_new(fs, NULL);
    if (volume == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(volume);

    vnode_t* vnode = vnode_new(volume, &rootClass);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(volume, NULL, NULL);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }

    dentry_make_positive(dentry, vnode);

    volume->root = dentry;
    *out = volume->root;
    return OK;
}

static filesystem_t procfs = {
    .name = PROCFS_NAME,
    .mount = procfs_mount,
};

void procfs_init(void)
{
    if (IS_ERR(filesystem_register(&procfs)))
    {
        panic(NULL, "Failed to register procfs filesystem");
    }
}