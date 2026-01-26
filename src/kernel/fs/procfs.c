#include <_libstd/MAX_NAME.h>
#include <kernel/fs/procfs.h>

#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <kernel/sync/rcu.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/status.h>

static bool procfs_revalidate_hide(dentry_t* dentry)
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

static dentry_ops_t hideDentryOps = {
    .revalidate = procfs_revalidate_hide,
};

static status_t procfs_prio_read(
    file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    priority_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return buffer_read(buffer, count, offset, bytesRead, prioStr, length);
}

static status_t procfs_prio_write(
    file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    process_t* process = file->vnode->data;

    char prioStr[MAX_NAME];
    if (count >= MAX_NAME)
    {
        return ERR(FS, INVAL);
    }

    memcpy(prioStr, buffer, count);
    prioStr[count] = '\0';

    long long int prio = atoll(prioStr);
    if (prio < 0)
    {
        return ERR(FS, INVAL);
    }
    if (prio > PRIORITY_MAX_USER)
    {
        return ERR(FS, ACCESS);
    }

    atomic_store(&process->priority, prio);
    *bytesWritten = count;
    return OK;
}

static file_ops_t prioOps = {
    .read = procfs_prio_read,
    .write = procfs_prio_write,
};

static status_t procfs_cwd_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    namespace_t* ns = process_get_ns(process);
    UNREF_DEFER(ns);

    path_t cwd = cwd_get(&process->cwd, ns);
    PATH_DEFER(&cwd);

    pathname_t cwdName;
    status_t status = path_to_name(&cwd, &cwdName);
    if (IS_ERR(status))
    {
        return status;
    }

    size_t length = strlen(cwdName.string);
    return buffer_read(buffer, count, offset, bytesRead, cwdName.string, length);
}

static status_t procfs_cwd_write(
    file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    process_t* process = file->vnode->data;

    char cwdStr[MAX_PATH];
    if (count >= MAX_PATH)
    {
        return ERR(FS, INVAL);
    }

    memcpy(cwdStr, buffer, count);
    cwdStr[count] = '\0';

    pathname_t cwdPathname;
    status_t status = pathname_init(&cwdPathname, cwdStr);
    if (IS_ERR(status))
    {
        return status;
    }

    namespace_t* ns = process_get_ns(process);
    UNREF_DEFER(ns);

    path_t path = cwd_get(&process->cwd, ns);
    PATH_DEFER(&path);

    status = path_walk(&path, &cwdPathname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        return ERR(FS, NOENT);
    }

    if (!DENTRY_IS_DIR(path.dentry))
    {
        return ERR(FS, NOTDIR);
    }

    cwd_set(&process->cwd, &path);
    *bytesWritten = count;
    return OK;
}

static file_ops_t cwdOps = {
    .read = procfs_cwd_read,
    .write = procfs_cwd_write,
};

static status_t procfs_cmdline_read(
    file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    if (process->argv == NULL || process->argc == 0)
    {
        *bytesRead = 0;
        return OK;
    }

    size_t totalSize = 0;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        totalSize += strlen(process->argv[i]) + 1;
    }

    if (totalSize == 0)
    {
        *bytesRead = 0;
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

    status_t status = buffer_read(buffer, count, offset, bytesRead, cmdline, totalSize);
    free(cmdline);
    return status;
}

static file_ops_t cmdlineOps = {
    .read = procfs_cmdline_read,
};

static status_t procfs_note_write(
    file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    if (count == 0)
    {
        *bytesWritten = 0;
        return OK;
    }

    if (count >= NOTE_MAX)
    {
        return ERR(FS, INVAL);
    }

    process_t* process = file->vnode->data;

    RCU_READ_SCOPE();

    thread_t* thread = process_rcu_first_thread(process);
    if (thread == NULL)
    {
        return ERR(FS, INVAL);
    }

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, count);
    status_t status = thread_send_note(thread, string);
    if (IS_ERR(status))
    {
        return status;
    }

    *bytesWritten = count;
    return OK;
}

static file_ops_t noteOps = {
    .write = procfs_note_write,
};

static status_t procfs_notegroup_write(
    file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    if (count == 0)
    {
        *bytesWritten = 0;
        return OK;
    }

    if (count >= NOTE_MAX)
    {
        return ERR(FS, INVAL);
    }
    process_t* process = file->vnode->data;

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, count);
    status_t status = group_send_note(&process->group, string);
    if (IS_ERR(status))
    {
        return status;
    }

    *bytesWritten = count;
    return OK;
}

static file_ops_t notegroupOps = {
    .write = procfs_notegroup_write,
};

static status_t procfs_group_open(file_t* file)
{
    process_t* process = file->vnode->data;

    group_t* group = group_get(&process->group);
    if (group == NULL)
    {
        return ERR(FS, NOGROUP);
    }

    file->data = group;
    return OK;
}

static void procfs_group_close(file_t* file)
{
    group_t* group = file->data;
    if (group == NULL)
    {
        return;
    }

    UNREF(group);
    file->data = NULL;
}

static file_ops_t groupOps = {
    .open = procfs_group_open,
    .close = procfs_group_close,
};

static status_t procfs_pid_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    char pidStr[MAX_NAME];
    uint32_t length = snprintf(pidStr, MAX_NAME, "%llu", process->id);
    return buffer_read(buffer, count, offset, bytesRead, pidStr, length);
}

static file_ops_t pidOps = {
    .read = procfs_pid_read,
};

static status_t procfs_wait_read(
    file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    status_t status = WAIT_BLOCK(&process->dyingQueue, atomic_load(&process->flags) & PROCESS_DYING);
    if (IS_ERR(status))
    {
        return status;
    }

    lock_acquire(&process->status.lock);
    status = buffer_read(
        buffer, count, offset, bytesRead, process->status.buffer, strlen(process->status.buffer));
    lock_release(&process->status.lock);
    return status;
}

static status_t procfs_wait_poll(file_t* file, poll_events_t* revents, wait_queue_t** queue)
{
    process_t* process = file->vnode->data;
    if (atomic_load(&process->flags) & PROCESS_DYING)
    {
        *revents |= POLLIN;
        *queue = &process->dyingQueue;
        return OK;
    }

    *queue = &process->dyingQueue;
    return OK;
}

static file_ops_t waitOps = {
    .read = procfs_wait_read,
    .poll = procfs_wait_poll,
};

static status_t procfs_perf_read(
    file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;
    size_t userPages = space_user_page_count(&process->space);

    RCU_READ_SCOPE();

    size_t threadCount = process_rcu_thread_count(process);

    clock_t userClocks = atomic_load(&process->perf.userClocks);
    clock_t kernelClocks = atomic_load(&process->perf.kernelClocks);
    clock_t startTime = process->perf.startTime;

    char statStr[MAX_PATH];
    int length = snprintf(statStr, sizeof(statStr),
        "user_clocks %llu\nkernel_sched_clocks %llu\nstart_clocks %llu\nuser_pages %llu\nthread_count %llu",
        userClocks, kernelClocks, startTime, userPages, threadCount);
    if (length < 0)
    {
        return ERR(FS, IMPL);
    }

    return buffer_read(buffer, count, offset, bytesRead, statStr, (size_t)length);
}

static file_ops_t perfOps = {
    .read = procfs_perf_read,
};

static status_t procfs_ns_open(file_t* file)
{
    process_t* process = file->vnode->data;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(FS, NOENT);
    }

    file->data = ns;
    return OK;
}

static void procfs_ns_close(file_t* file)
{
    if (file->data == NULL)
    {
        return;
    }

    UNREF(file->data);
    file->data = NULL;
}

static file_ops_t nsOps = {
    .open = procfs_ns_open,
    .close = procfs_ns_close,
};

static status_t procfs_ctl_close(file_t* file, uint64_t argc, const char** argv)
{
    process_t* process = file->vnode->data;

    if (argc == 2)
    {
        fd_t fd;
        if (sscanf(argv[1], "%lld", &fd) != 1)
        {
            return ERR(FS, INVAL);
        }

        return file_table_close(&process->files, fd);
    }

    fd_t minFd;
    if (sscanf(argv[1], "%lld", &minFd) != 1)
    {
        return ERR(FS, INVAL);
    }

    fd_t maxFd;
    if (sscanf(argv[2], "%lld", &maxFd) != 1)
    {
        return ERR(FS, INVAL);
    }

    file_table_close_range(&process->files, minFd, maxFd);
    return OK;
}

static status_t procfs_ctl_dup(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;

    fd_t oldFd;
    if (sscanf(argv[1], "%lld", &oldFd) != 1)
    {
        return ERR(FS, INVAL);
    }

    fd_t newFd;
    if (sscanf(argv[2], "%lld", &newFd) != 1)
    {
        return ERR(FS, INVAL);
    }

    return file_table_dup(&process->files, oldFd, &newFd);
}

static status_t procfs_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;
    process_t* writing = process_current();

    pathname_t targetName;
    status_t status = pathname_init(&targetName, argv[1]);
    if (IS_ERR(status))
    {
        return status;
    }

    namespace_t* processNs = process_get_ns(process);
    UNREF_DEFER(processNs);

    path_t target = cwd_get(&process->cwd, processNs);
    PATH_DEFER(&target);

    status = path_walk(&target, &targetName, processNs);
    if (IS_ERR(status))
    {
        return status;
    }

    pathname_t sourceName;
    status = pathname_init(&sourceName, argv[2]);
    if (IS_ERR(status))
    {
        return status;
    }

    namespace_t* writingNs = process_get_ns(writing);
    UNREF_DEFER(writingNs);

    path_t source = cwd_get(&writing->cwd, writingNs);
    PATH_DEFER(&source);

    status = path_walk(&source, &sourceName, writingNs);
    if (IS_ERR(status))
    {
        return status;
    }

    return namespace_bind(processNs, &target, &source, targetName.mode, NULL);
}

static status_t procfs_ctl_mount(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3 && argc != 4)
    {
        return ERR(FS, ARGC);
    }

    process_t* writing = process_current();
    process_t* process = file->vnode->data;

    pathname_t mountname;
    status_t status = pathname_init(&mountname, argv[1]);
    if (IS_ERR(status))
    {
        return status;
    }

    namespace_t* ns = process_get_ns(process);
    UNREF_DEFER(ns);

    path_t mountpath = cwd_get(&process->cwd, ns);
    PATH_DEFER(&mountpath);

    status = path_walk(&mountpath, &mountname, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    filesystem_t* fs = filesystem_get_by_path(argv[2], writing);
    if (fs == NULL)
    {
        return ERR(FS, NOFS);
    }

    const char* options = (argc == 4) ? argv[3] : NULL;
    return namespace_mount(ns, &mountpath, fs, options, mountname.mode, NULL, NULL);
}

static status_t procfs_ctl_touch(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;

    pathname_t pathname;
    status_t status = pathname_init(&pathname, argv[1]);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* touch;
    status = vfs_open(&touch, &pathname, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF(touch);
    return OK;
}

static status_t procfs_ctl_start(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argv);

    if (argc != 1)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;

    atomic_fetch_and(&process->flags, ~PROCESS_SUSPENDED);
    wait_unblock(&process->suspendQueue, WAIT_ALL, OK);

    return OK;
}

static status_t procfs_ctl_kill(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argv);

    process_t* process = file->vnode->data;

    if (argc == 2)
    {
        process_kill(process, argv[1]);
        return OK;
    }

    process_kill(process, "killed");

    return OK;
}

static status_t procfs_ctl_setns(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;

    fd_t fd;
    if (sscanf(argv[1], "%llu", &fd) != 1)
    {
        return ERR(FS, INVAL);
    }

    file_t* nsFile = file_table_get(&process->files, fd);
    if (nsFile == NULL)
    {
        return ERR(FS, BADFD);
    }
    UNREF_DEFER(nsFile);

    if (nsFile->ops != &nsOps)
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

static status_t procfs_ctl_setgroup(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        return ERR(FS, ARGC);
    }

    process_t* process = file->vnode->data;

    fd_t fd;
    if (sscanf(argv[1], "%llu", &fd) != 1)
    {
        return ERR(FS, INVAL);
    }

    file_t* groupFile = file_table_get(&process->files, fd);
    if (groupFile == NULL)
    {
        return ERR(FS, BADFD);
    }
    UNREF_DEFER(groupFile);

    if (groupFile->ops != &groupOps)
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

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {
        {"close", procfs_ctl_close, 2, 3},
        {"dup", procfs_ctl_dup, 3, 3},
        {"bind", procfs_ctl_bind, 3, 3},
        {"mount", procfs_ctl_mount, 3, 4},
        {"touch", procfs_ctl_touch, 2, 2},
        {"start", procfs_ctl_start, 1, 1},
        {"kill", procfs_ctl_kill, 1, 2},
        {"setns", procfs_ctl_setns, 2, 2},
        {"setgroup", procfs_ctl_setgroup, 2, 2},
        {0},
    })

static status_t procfs_env_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    process_t* process = file->vnode->data;

    const char* value = env_get(&process->env, file->path.dentry->name);
    if (value == NULL)
    {
        *bytesRead = 0;
        return OK;
    }

    size_t length = strlen(value);
    return buffer_read(buffer, count, offset, bytesRead, value, length);
}

static status_t procfs_env_write(
    file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    process_t* process = file->vnode->data;

    char value[MAX_NAME];
    if (count >= MAX_NAME)
    {
        return ERR(FS, INVAL);
    }

    memcpy(value, buffer, count);
    value[count] = '\0';

    status_t status = env_set(&process->env, file->path.dentry->name, value);
    if (IS_ERR(status))
    {
        return status;
    }

    *bytesWritten = count;
    return OK;
}

static file_ops_t envVarOps = {
    .read = procfs_env_read,
    .write = procfs_env_write,
};

static status_t procfs_env_lookup(vnode_t* dir, dentry_t* target)
{
    UNUSED(target);

    process_t* process = dir->data;
    assert(process != NULL);

    if (env_get(&process->env, target->name) == NULL)
    {
        return INFO(FS, NEGATIVE);
    }

    vnode_t* vnode = vnode_new(dir->superblock, VREG, NULL, &envVarOps);
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

    vnode_t* vnode = vnode_new(dir->superblock, VREG, NULL, &envVarOps);
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

static vnode_ops_t envVnodeOps = {
    .lookup = procfs_env_lookup,
    .create = procfs_env_create,
    .remove = procfs_env_remove,
};

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

        if (!ctx->emit(ctx, process->env.vars[i].key, VREG))
        {
            return OK;
        }
    }

    return OK;
}

static dentry_ops_t envDentryOps = {
    .iterate = procfs_env_iterate,
    .revalidate = procfs_revalidate_hide,
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

static vnode_ops_t selfOps = {
    .readlink = procfs_self_readlink,
};

typedef struct
{
    const char* name;
    vtype_t type;
    const vnode_ops_t* vnodeOps;
    const file_ops_t* fileOps;
    const dentry_ops_t* dentryOps;
} procfs_entry_t;

static const procfs_entry_t pidEntries[] = {
    {
        .name = "prio",
        .type = VREG,
        .fileOps = &prioOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "cwd",
        .type = VREG,
        .fileOps = &cwdOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "cmdline",
        .type = VREG,
        .fileOps = &cmdlineOps,
    },
    {
        .name = "note",
        .type = VREG,
        .fileOps = &noteOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "notegroup",
        .type = VREG,
        .fileOps = &notegroupOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "group",
        .type = VREG,
        .fileOps = &groupOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "pid",
        .type = VREG,
        .fileOps = &pidOps,
    },
    {
        .name = "wait",
        .type = VREG,
        .fileOps = &waitOps,
    },
    {
        .name = "perf",
        .type = VREG,
        .fileOps = &perfOps,
    },
    {
        .name = "ns",
        .type = VREG,
        .fileOps = &nsOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "ctl",
        .type = VREG,
        .fileOps = &ctlOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "env",
        .type = VDIR,
        .vnodeOps = &envVnodeOps,
        .dentryOps = &envDentryOps,
    },
};

static procfs_entry_t procEntries[] = {
    {
        .name = "self",
        .type = VSYMLINK,
        .vnodeOps = &selfOps,
        .fileOps = NULL,
    },
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

        vnode_t* vnode = vnode_new(dir->superblock, pidEntries[i].type, pidEntries[i].vnodeOps, pidEntries[i].fileOps);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = process; // No reference

        if (pidEntries[i].dentryOps != NULL)
        {
            target->ops = pidEntries[i].dentryOps;
        }

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
        if (pidEntries[i].dentryOps != NULL && pidEntries[i].dentryOps->revalidate == procfs_revalidate_hide)
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

        if (!ctx->emit(ctx, pidEntries[i].name, pidEntries[i].type))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_ops_t pidVnodeOps = {
    .lookup = procfs_pid_lookup,
    .cleanup = procfs_pid_cleanup,
};

static dentry_ops_t pidDentryOps = {
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

        vnode_t* vnode =
            vnode_new(dir->superblock, procEntries[i].type, procEntries[i].vnodeOps, procEntries[i].fileOps);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);

        dentry_make_positive(target, vnode);
        return OK;
    }

    pid_t pid;
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

    vnode_t* vnode = vnode_new(dir->superblock, VDIR, &pidVnodeOps, NULL);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = REF(process);

    target->ops = &pidDentryOps;

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

        if (!ctx->emit(ctx, procEntries[i].name, procEntries[i].type))
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
        if (!ctx->emit(ctx, name, VDIR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_ops_t procVnodeOps = {
    .lookup = procfs_lookup,
};

static dentry_ops_t procDentryOps = {
    .iterate = procfs_iterate,
};

static status_t procfs_mount(filesystem_t* fs,  dentry_t** out, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        return ERR(FS, INVAL);
    }

    superblock_t* superblock = superblock_new(fs, NULL, NULL);
    if (superblock == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(superblock);

    vnode_t* vnode = vnode_new(superblock, VDIR, &procVnodeOps, NULL);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    dentry->ops = &procDentryOps;

    dentry_make_positive(dentry, vnode);

    superblock->root = dentry;
    *out = superblock->root;
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