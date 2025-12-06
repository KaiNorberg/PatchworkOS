#include <kernel/fs/vfs.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/cwd.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/key.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <kernel/cpu/regs.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static uint64_t vfs_create(path_t* outPath, const pathname_t* pathname, const path_t* from, namespace_t* ns)
{
    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (path_walk_parent_and_child(&parent, &target, pathname, from, WALK_NEGATIVE_IS_OK, ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    inode_t* dir = parent.dentry->inode;
    if (dir->ops == NULL || dir->ops->create == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&dir->mutex);

    if (!(atomic_load(&target.dentry->flags) & DENTRY_NEGATIVE))
    {
        if (pathname->mode & MODE_EXCLUSIVE)
        {
            errno = EEXIST;
            return ERR;
        }

        if ((pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
        {
            errno = ENOTDIR;
            return ERR;
        }

        if (!(pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
        {
            errno = EISDIR;
            return ERR;
        }

        path_copy(outPath, &target);
        return 0;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    if (dir->ops->create(dir, target.dentry, pathname->mode) == ERR)
    {
        return ERR;
    }

    if (atomic_load(&target.dentry->flags) & DENTRY_NEGATIVE)
    {
        errno = EIO;
        return ERR;
    }

    path_copy(outPath, &target);
    return 0;
}

static uint64_t vfs_open_lookup(path_t* outPath, const pathname_t* pathname, const path_t* from, namespace_t* namespace)
{
    path_t target = PATH_EMPTY;
    if (pathname->mode & MODE_CREATE)
    {
        if (vfs_create(&target, pathname, from, namespace) == ERR)
        {
            return ERR;
        }
    }
    else // Dont create dentry
    {
        if (path_walk(&target, pathname, from, WALK_NEGATIVE_IS_ERR, namespace) == ERR)
        {
            return ERR;
        }
    }
    PATH_DEFER(&target);

    if ((pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (!(pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
    {
        errno = EISDIR;
        return ERR;
    }

    path_copy(outPath, &target);
    return 0;
}

file_t* vfs_open(const pathname_t* pathname, process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    return vfs_openat(&cwd, pathname, process);
}

uint64_t vfs_open2(const pathname_t* pathname, file_t* files[2], process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || files == NULL || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t path = PATH_EMPTY;
    if (vfs_open_lookup(&path, pathname, &cwd, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    files[0] = file_new(path.dentry->inode, &path, pathname->mode);
    if (files[0] == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(files[0]);

    files[1] = file_new(path.dentry->inode, &path, pathname->mode);
    if (files[1] == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(files[1]);

    if (pathname->mode & MODE_TRUNCATE && path.dentry->inode->type == INODE_FILE)
    {
        inode_truncate(path.dentry->inode);
    }

    if (files[0]->ops != NULL && files[0]->ops->open2 != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        uint64_t result = files[0]->ops->open2(files);
        if (result == ERR)
        {
            return ERR;
        }
    }

    inode_notify_access(files[0]->inode);
    REF(files[0]);
    REF(files[1]);
    return 0;
}

file_t* vfs_openat(const path_t* from, const pathname_t* pathname, process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    path_t path = PATH_EMPTY;
    if (vfs_open_lookup(&path, pathname, from, &process->ns) == ERR)
    {
        return NULL;
    }
    PATH_DEFER(&path);

    file_t* file = file_new(path.dentry->inode, &path, pathname->mode);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    if (pathname->mode & MODE_TRUNCATE && path.dentry->inode->type == INODE_FILE)
    {
        inode_truncate(path.dentry->inode);
    }

    if (file->ops != NULL && file->ops->open != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        uint64_t result = file->ops->open(file);
        if (result == ERR)
        {
            return NULL;
        }
    }

    inode_notify_access(file->inode);
    return REF(file);
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file == NULL || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->read == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if ((file->mode & MODE_APPEND) || !(file->mode & MODE_READ))
    {
        errno = EBADF;
        return ERR;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t offset = file->pos;
    uint64_t result = file->ops->read(file, buffer, count, &offset);
    file->pos = offset;

    if (result != ERR)
    {
        inode_notify_access(file->inode);
    }

    return result;
}

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count)
{
    if (file == NULL || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->write == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (file->mode & MODE_APPEND && file->ops->seek != NULL && file->ops->seek(file, 0, SEEK_END) == ERR)
    {
        return ERR;
    }

    if (!(file->mode & MODE_WRITE))
    {
        errno = EBADF;
        return ERR;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t offset = file->pos;
    uint64_t result = file->ops->write(file, buffer, count, &offset);
    file->pos = offset;

    if (result != ERR)
    {
        inode_notify_modify(file->inode);
    }

    return result;
}

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    if (file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->ops != NULL && file->ops->seek != NULL)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        return file->ops->seek(file, offset, origin);
    }

    errno = ESPIPE;
    return ERR;
}

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    if (file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->ioctl == NULL)
    {
        errno = ENOTTY;
        return ERR;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = file->ops->ioctl(file, request, argp, size);
    if (result != ERR)
    {
        inode_notify_access(file->inode);
    }
    return result;
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, pml_flags_t flags)
{
    if (file == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (file->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return NULL;
    }

    if (file->ops == NULL || file->ops->mmap == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t offset = file->pos;
    void* result = file->ops->mmap(file, address, length, &offset, flags);
    if (result != NULL)
    {
        inode_notify_access(file->inode);
        file->pos = offset;
    }
    return result;
}

typedef struct
{
    wait_queue_t* queues[CONFIG_MAX_FD];
    uint16_t lookupTable[CONFIG_MAX_FD];
    uint16_t queueAmount;
} vfs_poll_ctx_t;

static uint64_t vfs_poll_ctx_init(vfs_poll_ctx_t* ctx, poll_file_t* files, uint64_t amount)
{
    memset(ctx->queues, 0, sizeof(wait_queue_t*) * CONFIG_MAX_FD);
    memset(ctx->lookupTable, 0, sizeof(uint16_t) * CONFIG_MAX_FD);
    ctx->queueAmount = 0;

    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].revents = POLLNONE;
        wait_queue_t* queue = files[i].file->ops->poll(files[i].file, &files[i].revents);
        if (queue == NULL)
        {
            return ERR;
        }

        // Avoid duplicate queues.
        bool found = false;
        for (uint16_t j = 0; j < ctx->queueAmount; j++)
        {
            if (ctx->queues[j] == queue)
            {
                found = true;
                ctx->lookupTable[i] = j;
                break;
            }
        }

        if (!found)
        {
            ctx->queues[ctx->queueAmount] = queue;
            ctx->lookupTable[i] = ctx->queueAmount;
            ctx->queueAmount++;
        }
    }

    return 0;
}

static uint64_t vfs_poll_ctx_check_events(vfs_poll_ctx_t* ctx, poll_file_t* files, uint64_t amount)
{
    uint64_t readyCount = 0;

    for (uint64_t i = 0; i < amount; i++)
    {
        poll_events_t revents = POLLNONE;
        wait_queue_t* queue = files[i].file->ops->poll(files[i].file, &revents);
        if (queue == NULL)
        {
            return ERR;
        }

        files[i].revents = (revents & (files[i].events | POLL_SPECIAL));

        // Make sure the queue hasn't changed, just for debugging.
        if (queue != ctx->queues[ctx->lookupTable[i]])
        {
            errno = EIO;
            return ERR;
        }

        if ((files[i].revents & (files[i].events | POLL_SPECIAL)) != 0)
        {
            readyCount++;
        }
    }

    return readyCount;
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout)
{
    if (files == NULL || amount == 0 || amount > CONFIG_MAX_FD)
    {
        errno = EINVAL;
        return ERR;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (files[i].file == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        if (files[i].file->inode->type == INODE_DIR)
        {
            errno = EISDIR;
            return ERR;
        }

        if (files[i].file->ops == NULL || files[i].file->ops->poll == NULL)
        {
            errno = ENOSYS;
            return ERR;
        }
    }

    vfs_poll_ctx_t ctx;
    if (vfs_poll_ctx_init(&ctx, files, amount) == ERR)
    {
        return ERR;
    }

    clock_t uptime = sys_time_uptime();
    clock_t deadline = CLOCKS_DEADLINE(timeout, uptime);

    uint64_t readyCount = 0;
    while (true)
    {
        uptime = sys_time_uptime();
        clock_t remaining = CLOCKS_REMAINING(deadline, uptime);

        if (wait_block_prepare(ctx.queues, ctx.queueAmount, remaining) == ERR)
        {
            return ERR;
        }

        readyCount = vfs_poll_ctx_check_events(&ctx, files, amount);
        if (readyCount == ERR)
        {
            wait_block_cancel();
            return ERR;
        }

        if (readyCount > 0 || uptime >= deadline)
        {
            wait_block_cancel();
            break;
        }

        if (wait_block_commit() == ERR)
        {
            if (errno == ETIMEDOUT)
            {
                break;
            }
            return ERR;
        }
    }

    return readyCount;
}

uint64_t vfs_getdents(file_t* file, dirent_t* buffer, uint64_t count)
{
    if (file == NULL || (buffer == NULL && count > 0))
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->inode == NULL || file->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (file->path.dentry == NULL || file->path.dentry->parent == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->path.dentry->ops == NULL || file->path.dentry->ops->getdents == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (!(file->mode & MODE_READ))
    {
        errno = EBADF;
        return ERR;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = file->path.dentry->ops->getdents(file->path.dentry, buffer, count, &file->pos, file->mode);
    if (result != ERR)
    {
        inode_notify_access(file->inode);
    }
    return result;
}

uint64_t vfs_stat(const pathname_t* pathname, stat_t* buffer, process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || buffer == NULL || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (pathname->mode != MODE_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t path = PATH_EMPTY;
    if (path_walk(&path, pathname, &cwd, WALK_NEGATIVE_IS_ERR, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    if (!(path.mount->mode & MODE_READ))
    {
        errno = EACCES;
        return ERR;
    }

    memset(buffer, 0, sizeof(stat_t));

    MUTEX_SCOPE(&path.dentry->inode->mutex);
    buffer->number = path.dentry->inode->number;
    buffer->type = path.dentry->inode->type;
    buffer->size = path.dentry->inode->size;
    buffer->blocks = path.dentry->inode->blocks;
    buffer->linkAmount = path.dentry->inode->linkCount;
    buffer->accessTime = path.dentry->inode->accessTime;
    buffer->modifyTime = path.dentry->inode->modifyTime;
    buffer->changeTime = path.dentry->inode->changeTime;
    buffer->createTime = path.dentry->inode->createTime;
    strncpy(buffer->name, path.dentry->name, MAX_NAME - 1);
    buffer->name[MAX_NAME - 1] = '\0';

    return 0;
}

uint64_t vfs_link(const pathname_t* oldPathname, const pathname_t* newPathname, process_t* process)
{
    if (!PATHNAME_IS_VALID(oldPathname) || !PATHNAME_IS_VALID(newPathname) || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (oldPathname->mode != MODE_NONE || newPathname->mode != MODE_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t oldParent = PATH_EMPTY;
    path_t old = PATH_EMPTY;
    if (path_walk_parent_and_child(&oldParent, &old, oldPathname, &cwd, WALK_NEGATIVE_IS_ERR, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&oldParent);
    PATH_DEFER(&old);

    path_t newParent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (path_walk_parent_and_child(&newParent, &target, newPathname, &cwd, WALK_NEGATIVE_IS_OK, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&newParent);
    PATH_DEFER(&target);

    if (oldParent.dentry->superblock->id != newParent.dentry->superblock->id)
    {
        errno = EXDEV;
        return ERR;
    }

    if (newParent.dentry->inode == NULL || newParent.dentry->inode->ops == NULL ||
        newParent.dentry->inode->ops->link == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (!(old.mount->mode & MODE_READ))
    {
        errno = EACCES;
        return ERR;
    }

    if (!(newParent.mount->mode & MODE_WRITE))
    {
        errno = EACCES;
        return ERR;
    }

    mutex_acquire(&old.dentry->inode->mutex);
    mutex_acquire(&newParent.dentry->inode->mutex);

    uint64_t result = 0;
    if (!(atomic_load(&target.dentry->flags) & DENTRY_NEGATIVE))
    {
        errno = EEXIST;
        result = ERR;
    }

    if (result != ERR)
    {
        assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
        result = newParent.dentry->inode->ops->link(old.dentry, newParent.dentry->inode, target.dentry);
    }

    mutex_release(&newParent.dentry->inode->mutex);
    mutex_release(&old.dentry->inode->mutex);

    if (result == ERR)
    {
        return ERR;
    }

    inode_notify_modify(newParent.dentry->inode);
    inode_notify_change(old.dentry->inode);

    return 0;
}

uint64_t vfs_remove(const pathname_t* pathname, process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t cwd = cwd_get(&process->cwd);
    PATH_DEFER(&cwd);

    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (path_walk_parent_and_child(&parent, &target, pathname, &cwd, WALK_NEGATIVE_IS_ERR, &process->ns) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if ((pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (!(pathname->mode & MODE_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
    {
        errno = EISDIR;
        return ERR;
    }

    if (!(target.mount->mode & MODE_WRITE))
    {
        errno = EACCES;
        return ERR;
    }

    inode_t* dir = parent.dentry->inode;
    if (dir->ops == NULL || dir->ops->remove == NULL)
    {
        errno = EPERM;
        return ERR;
    }

    mutex_acquire(&dir->mutex);
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = dir->ops->remove(dir, target.dentry, pathname->mode);
    if (result != ERR)
    {
        inode_notify_change(target.dentry->inode);  
        dentry_make_negative(target.dentry);
    }
    mutex_release(&dir->mutex);

    return result;
}

uint64_t vfs_get_new_id(void)
{
    static _Atomic(uint64_t) newVfsId = ATOMIC_VAR_INIT(0);

    return atomic_fetch_add(&newVfsId, 1);
}

SYSCALL_DEFINE(SYS_OPEN, fd_t, const char* pathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    file_t* file = vfs_open(&pathname, process);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    return file_table_alloc(&process->fileTable, file);
}

SYSCALL_DEFINE(SYS_OPEN2, uint64_t, const char* pathString, fd_t fds[2])
{
    if (fds == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    file_t* files[2];
    if (vfs_open2(&pathname, files, process) == ERR)
    {
        return ERR;
    }
    DEREF_DEFER(files[0]);
    DEREF_DEFER(files[1]);

    fd_t fdsLocal[2];
    fdsLocal[0] = file_table_alloc(&process->fileTable, files[0]);
    if (fdsLocal[0] == ERR)
    {
        return ERR;
    }
    fdsLocal[1] = file_table_alloc(&process->fileTable, files[1]);
    if (fdsLocal[1] == ERR)
    {
        file_table_free(&process->fileTable, fdsLocal[0]);
        return ERR;
    }

    if (thread_copy_to_user(thread, fds, fdsLocal, sizeof(fd_t) * 2) == ERR)
    {
        file_table_free(&process->fileTable, fdsLocal[0]);
        file_table_free(&process->fileTable, fdsLocal[1]);
        return ERR;
    }

    return 0;
}

SYSCALL_DEFINE(SYS_OPENAT, fd_t, fd_t from, const char* pathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    path_t fromPath = PATH_EMPTY;
    if (from == FD_NONE)
    {
        path_t cwd = cwd_get(&process->cwd);
        path_copy(&fromPath, &cwd);
        path_put(&cwd);
    }
    else
    {
        file_t* fromFile = file_table_get(&process->fileTable, from);
        if (fromFile == NULL)
        {
            return ERR;
        }
        path_copy(&fromPath, &fromFile->path);
        DEREF(fromFile);
    }
    PATH_DEFER(&fromPath);

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    file_t* file = vfs_openat(&fromPath, &pathname, process);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    return file_table_alloc(&process->fileTable, file);
}

SYSCALL_DEFINE(SYS_READ, uint64_t, fd_t fd, void* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    if (space_pin(&process->space, buffer, count, &thread->userStack) == ERR)
    {
        return ERR;
    }
    uint64_t result = vfs_read(file, buffer, count);
    space_unpin(&process->space, buffer, count);
    return result;
}

SYSCALL_DEFINE(SYS_WRITE, uint64_t, fd_t fd, const void* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    if (space_pin(&process->space, buffer, count, &thread->userStack) == ERR)
    {
        return ERR;
    }
    uint64_t result = vfs_write(file, buffer, count);
    space_unpin(&process->space, buffer, count);
    return result;
}

SYSCALL_DEFINE(SYS_SEEK, uint64_t, fd_t fd, int64_t offset, seek_origin_t origin)
{
    process_t* process = sched_process();

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    return vfs_seek(file, offset, origin);
}

SYSCALL_DEFINE(SYS_IOCTL, uint64_t, fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    if (space_pin(&process->space, argp, size, &thread->userStack) == ERR)
    {
        return ERR;
    }
    uint64_t result = vfs_ioctl(file, request, argp, size);
    space_unpin(&process->space, argp, size);
    return result;
}

SYSCALL_DEFINE(SYS_MMAP, void*, fd_t fd, void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (address != NULL && space_check_access(space, address, length) == ERR)
    {
        return NULL;
    }

    pml_flags_t flags = vmm_prot_to_flags(prot);
    if (flags == PML_NONE)
    {
        errno = EINVAL;
        return NULL;
    }

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    if ((!(file->mode & MODE_READ) && (prot & PROT_READ)) || (!(file->mode & MODE_WRITE) && (prot & PROT_WRITE)) ||
        (!(file->mode & MODE_EXECUTE) && (prot & PROT_EXECUTE)))
    {
        errno = EACCES;
        return NULL;
    }

    return vfs_mmap(file, address, length, flags | PML_USER);
}

SYSCALL_DEFINE(SYS_POLL, uint64_t, pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    if (amount == 0 || amount >= CONFIG_MAX_FD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(&process->space, fds, sizeof(pollfd_t) * amount, &thread->userStack) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    poll_file_t files[CONFIG_MAX_FD];
    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].file = file_table_get(&process->fileTable, fds[i].fd);
        if (files[i].file == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                DEREF(files[j].file);
            }
            if (errno == EBADF)
            {
                fds[i].revents = POLLNVAL;
            }
            space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);
            return ERR;
        }

        files[i].events = fds[i].events;
        files[i].revents = POLLNONE;
    }

    uint64_t result = vfs_poll(files, amount, timeout);
    if (result != ERR)
    {
        for (uint64_t i = 0; i < amount; i++)
        {
            fds[i].revents = files[i].revents;
        }
    }
    space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);

    for (uint64_t i = 0; i < amount; i++)
    {
        DEREF(files[i].file);
    }

    return result;
}

SYSCALL_DEFINE(SYS_GETDENTS, uint64_t, fd_t fd, dirent_t* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    if (space_pin(&process->space, buffer, count, &thread->userStack) == ERR)
    {
        return ERR;
    }
    uint64_t result = vfs_getdents(file, buffer, count);
    space_unpin(&process->space, buffer, count);
    return result;
}

SYSCALL_DEFINE(SYS_STAT, uint64_t, const char* pathString, stat_t* buffer)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    if (space_pin(&process->space, buffer, sizeof(stat_t), &thread->userStack) == ERR)
    {
        return ERR;
    }
    uint64_t result = vfs_stat(&pathname, buffer, process);
    space_unpin(&process->space, buffer, sizeof(stat_t));
    return result;
}

SYSCALL_DEFINE(SYS_LINK, uint64_t, const char* oldPathString, const char* newPathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t oldPathname;
    if (thread_copy_from_user_pathname(thread, &oldPathname, oldPathString) == ERR)
    {
        return ERR;
    }

    pathname_t newPathname;
    if (thread_copy_from_user_pathname(thread, &newPathname, newPathString) == ERR)
    {
        return ERR;
    }

    return vfs_link(&oldPathname, &newPathname, process);
}

SYSCALL_DEFINE(SYS_REMOVE, uint64_t, const char* pathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    return vfs_remove(&pathname, process);
}