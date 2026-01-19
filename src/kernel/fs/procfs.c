#include <_internal/MAX_NAME.h>
#include <kernel/fs/procfs.h>

#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <errno.h>
#include <kernel/sync/rcu.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static uint64_t procfs_revalidate_hide(dentry_t* dentry)
{
    process_t* current = process_current();
    assert(current != NULL);
    process_t* process = dentry->inode->data;
    assert(process != NULL);

    namespace_t* currentNs = process_get_ns(current);
    if (currentNs == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(currentNs);

    namespace_t* processNs = process_get_ns(process);
    if (processNs == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(processNs);

    if (!namespace_accessible(currentNs, processNs))
    {
        errno = ENOENT;
        return ERR;
    }

    return 0;
}

static dentry_ops_t hideDentryOps = {
    .revalidate = procfs_revalidate_hide,
};

static size_t procfs_prio_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    priority_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return BUFFER_READ(buffer, count, offset, prioStr, length);
}

static size_t procfs_prio_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    process_t* process = file->inode->data;

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
    .read = procfs_prio_read,
    .write = procfs_prio_write,
};

static size_t procfs_cwd_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t cwd = cwd_get(&process->cwd, ns);
    PATH_DEFER(&cwd);

    pathname_t cwdName;
    if (path_to_name(&cwd, &cwdName) == ERR)
    {
        return ERR;
    }

    size_t length = strlen(cwdName.string);
    return BUFFER_READ(buffer, count, offset, cwdName.string, length);
}

static size_t procfs_cwd_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    process_t* process = file->inode->data;

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

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t path = cwd_get(&process->cwd, ns);
    PATH_DEFER(&path);

    if (path_walk(&path, &cwdPathname, ns) == ERR)
    {
        return ERR;
    }

    if (!DENTRY_IS_POSITIVE(path.dentry))
    {
        errno = ENOENT;
        return ERR;
    }

    if (!DENTRY_IS_DIR(path.dentry))
    {
        errno = ENOTDIR;
        return ERR;
    }

    cwd_set(&process->cwd, &path);
    return count;
}

static file_ops_t cwdOps = {
    .read = procfs_cwd_read,
    .write = procfs_cwd_write,
};

static size_t procfs_cmdline_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    if (process->argv == NULL || process->argc == 0)
    {
        return 0;
    }

    size_t totalSize = 0;
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

    size_t result = BUFFER_READ(buffer, count, offset, cmdline, totalSize);
    free(cmdline);
    return result;
}

static file_ops_t cmdlineOps = {
    .read = procfs_cmdline_read,
};

static size_t procfs_note_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    if (count == 0)
    {
        return 0;
    }

    if (count >= NOTE_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

    RCU_READ_SCOPE();

    thread_t* thread = process_rcu_first_thread(process);
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
    .write = procfs_note_write,
};

static size_t procfs_notegroup_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    if (count == 0)
    {
        return 0;
    }

    if (count >= NOTE_MAX)
    {
        errno = EINVAL;
        return ERR;
    }
    process_t* process = file->inode->data;

    char string[NOTE_MAX] = {0};
    memcpy_s(string, NOTE_MAX, buffer, count);
    if (group_send_note(&process->group, string) == ERR)
    {
        return ERR;
    }

    return count;
}

static file_ops_t notegroupOps = {
    .write = procfs_notegroup_write,
};

static uint64_t procfs_group_open(file_t* file)
{
    process_t* process = file->inode->data;

    group_t* group = group_get(&process->group);
    if (group == NULL)
    {
        return ERR;
    }

    file->data = group;
    return 0;
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

static size_t procfs_pid_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    char pidStr[MAX_NAME];
    uint32_t length = snprintf(pidStr, MAX_NAME, "%llu", process->id);
    return BUFFER_READ(buffer, count, offset, pidStr, length);
}

static file_ops_t pidOps = {
    .read = procfs_pid_read,
};

static size_t procfs_wait_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    if (WAIT_BLOCK(&process->dyingQueue, atomic_load(&process->flags) & PROCESS_DYING) == ERR)
    {
        return ERR;
    }

    lock_acquire(&process->status.lock);
    size_t result = BUFFER_READ(buffer, count, offset, process->status.buffer, strlen(process->status.buffer));
    lock_release(&process->status.lock);
    return result;
}

static wait_queue_t* procfs_wait_poll(file_t* file, poll_events_t* revents)
{
    process_t* process = file->inode->data;
    if (atomic_load(&process->flags) & PROCESS_DYING)
    {
        *revents |= POLLIN;
        return &process->dyingQueue;
    }

    return &process->dyingQueue;
}

static file_ops_t waitOps = {
    .read = procfs_wait_read,
    .poll = procfs_wait_poll,
};

static size_t procfs_perf_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;
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
        errno = EIO;
        return ERR;
    }

    return BUFFER_READ(buffer, count, offset, statStr, (size_t)length);
}

static file_ops_t perfOps = {
    .read = procfs_perf_read,
};

static uint64_t procfs_ns_open(file_t* file)
{
    process_t* process = file->inode->data;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }

    file->data = ns;
    return 0;
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

static uint64_t procfs_ctl_close(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2 && argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

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

static uint64_t procfs_ctl_dup2(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

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

static uint64_t procfs_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;
    process_t* writing = process_current();

    pathname_t targetName;
    if (pathname_init(&targetName, argv[1]) == ERR)
    {
        return ERR;
    }

    namespace_t* processNs = process_get_ns(process);
    if (processNs == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(processNs);

    path_t target = cwd_get(&process->cwd, processNs);
    PATH_DEFER(&target);

    if (path_walk(&target, &targetName, processNs) == ERR)
    {
        return ERR;
    }

    pathname_t sourceName;
    if (pathname_init(&sourceName, argv[2]) == ERR)
    {
        return ERR;
    }

    namespace_t* writingNs = process_get_ns(writing);
    if (writingNs == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(writingNs);

    path_t source = cwd_get(&writing->cwd, writingNs);
    PATH_DEFER(&source);

    if (path_walk(&source, &sourceName, writingNs) == ERR)
    {
        return ERR;
    }

    mount_t* mount = namespace_bind(processNs, &target, &source, targetName.mode);
    if (mount == NULL)
    {
        return ERR;
    }
    UNREF(mount);
    return 0;
}

static uint64_t procfs_ctl_mount(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 3 && argc != 4)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* writing = process_current();
    process_t* process = file->inode->data;

    pathname_t mountname;
    if (pathname_init(&mountname, argv[1]) == ERR)
    {
        return ERR;
    }

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    path_t mountpath = cwd_get(&process->cwd, ns);
    PATH_DEFER(&mountpath);

    if (path_walk(&mountpath, &mountname, ns) == ERR)
    {
        return ERR;
    }

    filesystem_t* fs = filesystem_get_by_path(argv[2], writing);
    if (fs == NULL)
    {
        return ERR;
    }

    const char* options = (argc == 4) ? argv[3] : NULL;
    mount_t* mount = namespace_mount(ns, &mountpath, fs, options, mountname.mode, NULL);
    if (mount == NULL)
    {
        return ERR;
    }
    UNREF(mount);
    return 0;
}

static uint64_t procfs_ctl_touch(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

    pathname_t pathname;
    if (pathname_init(&pathname, argv[1]) == ERR)
    {
        return ERR;
    }

    file_t* touch = vfs_open(&pathname, process);
    if (touch == NULL)
    {
        return ERR;
    }
    UNREF(touch);
    return 0;
}

static uint64_t procfs_ctl_start(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argv);

    if (argc != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

    atomic_fetch_and(&process->flags, ~PROCESS_SUSPENDED);
    wait_unblock(&process->suspendQueue, WAIT_ALL, EOK);

    return 0;
}

static uint64_t procfs_ctl_kill(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argv);

    process_t* process = file->inode->data;

    if (argc == 2)
    {
        process_kill(process, argv[1]);
        return 0;
    }

    process_kill(process, "killed");

    return 0;
}

static uint64_t procfs_ctl_setns(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

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

    namespace_t* ns = nsFile->data;
    if (ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    process_set_ns(process, ns);
    return 0;
}

static uint64_t procfs_ctl_setgroup(file_t* file, uint64_t argc, const char** argv)
{
    if (argc != 2)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = file->inode->data;

    fd_t fd;
    if (sscanf(argv[1], "%llu", &fd) != 1)
    {
        errno = EINVAL;
        return ERR;
    }

    file_t* groupFile = file_table_get(&process->fileTable, fd);
    if (groupFile == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(groupFile);

    if (groupFile->ops != &groupOps)
    {
        errno = EINVAL;
        return ERR;
    }

    group_t* target = groupFile->data;
    if (target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    group_add(target, &process->group);
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {
        {"close", procfs_ctl_close, 2, 3},
        {"dup2", procfs_ctl_dup2, 3, 3},
        {"bind", procfs_ctl_bind, 3, 3},
        {"mount", procfs_ctl_mount, 3, 4},
        {"touch", procfs_ctl_touch, 2, 2},
        {"start", procfs_ctl_start, 1, 1},
        {"kill", procfs_ctl_kill, 1, 2},
        {"setns", procfs_ctl_setns, 2, 2},
        {"setgroup", procfs_ctl_setgroup, 2, 2},
        {0},
    })

static size_t procfs_env_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    process_t* process = file->inode->data;

    const char* value = env_get(&process->env, file->path.dentry->name);
    if (value == NULL)
    {
        return 0;
    }

    size_t length = strlen(value);
    return BUFFER_READ(buffer, count, offset, value, length);
}

static size_t procfs_env_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(offset);

    process_t* process = file->inode->data;

    char value[MAX_NAME];
    if (count >= MAX_NAME)
    {
        errno = EINVAL;
        return ERR;
    }

    memcpy(value, buffer, count);
    value[count] = '\0';

    if (env_set(&process->env, file->path.dentry->name, value) == ERR)
    {
        return ERR;
    }

    return count;
}

static file_ops_t envVarOps = {
    .read = procfs_env_read,
    .write = procfs_env_write,
};

static uint64_t procfs_env_lookup(inode_t* dir, dentry_t* target)
{
    UNUSED(target);

    process_t* process = dir->data;
    assert(process != NULL);

    if (env_get(&process->env, target->name) == NULL)
    {
        return 0;
    }

    inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, target->name), INODE_REGULAR, NULL, &envVarOps);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);
    inode->data = process; // No reference

    dentry_make_positive(target, inode);

    return 0;
}

static uint64_t procfs_env_create(inode_t* dir, dentry_t* target, mode_t mode)
{
    if (mode & MODE_DIRECTORY)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = dir->data;
    assert(process != NULL);

    if (env_set(&process->env, target->name, "") == ERR)
    {
        return ERR;
    }

    inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, target->name), INODE_REGULAR, NULL, &envVarOps);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);
    inode->data = process; // No reference

    dentry_make_positive(target, inode);
    return 0;
}

static uint64_t procfs_env_remove(inode_t* dir, dentry_t* target)
{
    process_t* process = dir->data;
    assert(process != NULL);

    if (env_unset(&process->env, target->name) == ERR)
    {
        return ERR;
    }

    return 0;
}

static inode_ops_t envInodeOps = {
    .lookup = procfs_env_lookup,
    .create = procfs_env_create,
    .remove = procfs_env_remove,
};

static uint64_t procfs_env_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    process_t* process = dentry->inode->data;
    assert(process != NULL);

    MUTEX_SCOPE(&process->env.mutex);

    for (size_t i = 0; i < process->env.count; i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, process->env.vars[i].key, ino_gen(dentry->inode->number, process->env.vars[i].key),
                INODE_REGULAR))
        {
            return 0;
        }
    }

    return 0;
}

static dentry_ops_t envDentryOps = {
    .iterate = procfs_env_iterate,
    .revalidate = procfs_revalidate_hide,
};

static uint64_t procfs_self_readlink(inode_t* inode, char* buffer, uint64_t count)
{
    UNUSED(inode);

    process_t* process = process_current();
    int ret = snprintf(buffer, count, "%llu", process->id);
    if (ret < 0 || ret >= (int)count)
    {
        return ERR;
    }

    return ret;
}

static inode_ops_t selfOps = {
    .readlink = procfs_self_readlink,
};

typedef struct
{
    const char* name;
    itype_t type;
    const inode_ops_t* inodeOps;
    const file_ops_t* fileOps;
    const dentry_ops_t* dentryOps;
} procfs_entry_t;

static const procfs_entry_t pidEntries[] = {
    {
        .name = "prio",
        .type = INODE_REGULAR,
        .fileOps = &prioOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "cwd",
        .type = INODE_REGULAR,
        .fileOps = &cwdOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "cmdline",
        .type = INODE_REGULAR,
        .fileOps = &cmdlineOps,
    },
    {
        .name = "note",
        .type = INODE_REGULAR,
        .fileOps = &noteOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "notegroup",
        .type = INODE_REGULAR,
        .fileOps = &notegroupOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "group",
        .type = INODE_REGULAR,
        .fileOps = &groupOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "pid",
        .type = INODE_REGULAR,
        .fileOps = &pidOps,
    },
    {
        .name = "wait",
        .type = INODE_REGULAR,
        .fileOps = &waitOps,
    },
    {
        .name = "perf",
        .type = INODE_REGULAR,
        .fileOps = &perfOps,
    },
    {
        .name = "ns",
        .type = INODE_REGULAR,
        .fileOps = &nsOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "ctl",
        .type = INODE_REGULAR,
        .fileOps = &ctlOps,
        .dentryOps = &hideDentryOps,
    },
    {
        .name = "env",
        .type = INODE_DIR,
        .inodeOps = &envInodeOps,
        .dentryOps = &envDentryOps,
    },
};

static procfs_entry_t procEntries[] = {
    {
        .name = "self",
        .type = INODE_SYMLINK,
        .inodeOps = &selfOps,
        .fileOps = NULL,
    },
};

static uint64_t procfs_pid_lookup(inode_t* dir, dentry_t* target)
{
    process_t* process = dir->data;
    assert(process != NULL);

    for (size_t i = 0; i < ARRAY_SIZE(pidEntries); i++)
    {
        if (strcmp(target->name, pidEntries[i].name) != 0)
        {
            continue;
        }

        inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, pidEntries[i].name), pidEntries[i].type,
            pidEntries[i].inodeOps, pidEntries[i].fileOps);
        if (inode == NULL)
        {
            return 0;
        }
        UNREF_DEFER(inode);
        inode->data = process; // No reference

        if (pidEntries[i].dentryOps != NULL)
        {
            target->ops = pidEntries[i].dentryOps;
        }

        dentry_make_positive(target, inode);
        return 0;
    }

    return 0;
}

static void procfs_pid_cleanup(inode_t* inode)
{
    process_t* process = inode->data;
    if (process == NULL)
    {
        return;
    }

    UNREF(process);
    inode->data = NULL;
}

static uint64_t procfs_pid_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    process_t* current = process_current();
    process_t* process = dentry->inode->data;
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

        if (!ctx->emit(ctx, pidEntries[i].name, ino_gen(dentry->inode->number, pidEntries[i].name), pidEntries[i].type))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t pidInodeOps = {
    .lookup = procfs_pid_lookup,
    .cleanup = procfs_pid_cleanup,
};

static dentry_ops_t pidDentryOps = {
    .iterate = procfs_pid_iterate,
};

static uint64_t procfs_lookup(inode_t* dir, dentry_t* target)
{
    for (size_t i = 0; i < ARRAY_SIZE(procEntries); i++)
    {
        if (strcmp(target->name, procEntries[i].name) != 0)
        {
            continue;
        }

        inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, target->name), procEntries[i].type,
            procEntries[i].inodeOps, procEntries[i].fileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);

        dentry_make_positive(target, inode);
        return 0;
    }

    pid_t pid;
    if (sscanf(target->name, "%llu", &pid) != 1)
    {
        return 0;
    }

    process_t* process = process_get(pid);
    if (process == NULL)
    {
        return 0;
    }
    UNREF_DEFER(process);

    inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, target->name), INODE_DIR, &pidInodeOps, NULL);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);
    inode->data = REF(process);

    target->ops = &pidDentryOps;

    dentry_make_positive(target, inode);
    return 0;
}

static uint64_t procfs_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(procEntries); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, procEntries[i].name, ino_gen(dentry->inode->number, procEntries[i].name),
                procEntries[i].type))
        {
            return 0;
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
        if (!ctx->emit(ctx, name, ino_gen(dentry->inode->number, name), INODE_DIR))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t procInodeOps = {
    .lookup = procfs_lookup,
};

static dentry_ops_t procDentryOps = {
    .iterate = procfs_iterate,
};

static dentry_t* procfs_mount(filesystem_t* fs, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    superblock_t* superblock = superblock_new(fs, NULL, NULL);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, 0, INODE_DIR, &procInodeOps, NULL);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return NULL;
    }
    dentry->ops = &procDentryOps;

    dentry_make_positive(dentry, inode);

    superblock->root = dentry;
    return superblock->root;
}

static filesystem_t procfs = {
    .name = PROCFS_NAME,
    .mount = procfs_mount,
};

void procfs_init(void)
{
    if (filesystem_register(&procfs) == ERR)
    {
        panic(NULL, "Failed to register procfs filesystem");
    }
}