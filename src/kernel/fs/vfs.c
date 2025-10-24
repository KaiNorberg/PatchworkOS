#include "vfs.h"

#include "cpu/syscalls.h"
#include "fs/dentry.h"
#include "fs/inode.h"
#include "fs/mount.h"
#include "fs/path.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "sys/list.h"
#include "sysfs.h"
#include "utils/ref.h"
#include "vfs_ctx.h"

#include <common/regs.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

static _Atomic(uint64_t) newVfsId = ATOMIC_VAR_INIT(0);

static vfs_list_t superblocks;
static vfs_list_t filesystems;

static vfs_map_t dentryCache;
static vfs_map_t inodeCache;

static dentry_t* root = NULL;

static map_key_t inode_cache_key(superblock_id_t superblockId, inode_number_t number)
{
    uint64_t buffer[2] = {(uint64_t)superblockId, (uint64_t)number};

    return map_key_buffer(buffer, sizeof(buffer));
}

static map_key_t dentry_cache_key(dentry_id_t parentId, const char* name)
{
    char buffer[MAX_PATH] = {0};
    memcpy(buffer, &parentId, sizeof(dentry_id_t));
    uint64_t offset = sizeof(dentry_id_t);

    uint64_t nameLen = strlen(name);
    assert(offset + nameLen < MAX_PATH);
    memcpy(buffer + offset, name, nameLen);
    offset += nameLen;

    return map_key_buffer(buffer, offset);
}

static void vfs_list_init(vfs_list_t* list)
{
    list_init(&list->list);
    rwlock_init(&list->lock);
}

static void vfs_map_init(vfs_map_t* map)
{
    if (map_init(&map->map) == ERR)
    {
        panic(NULL, "vfs: failed to initialize map");
    }
    rwlock_init(&map->lock);
}

void vfs_init(void)
{
    vfs_list_init(&superblocks);
    vfs_list_init(&filesystems);

    vfs_map_init(&dentryCache);
    vfs_map_init(&inodeCache);

    path_flags_init();

    LOG_INFO("virtual file system initialized\n");
}

uint64_t vfs_get_new_id(void)
{
    return atomic_fetch_add(&newVfsId, 1);
}

uint64_t vfs_register_fs(filesystem_t* fs)
{
    if (fs == NULL || strnlen_s(fs->name, MAX_NAME) > MAX_NAME)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&filesystems.lock);

    filesystem_t* existing;
    LIST_FOR_EACH(existing, &filesystems.list, entry)
    {
        if (strcmp(existing->name, fs->name) == 0)
        {
            errno = EEXIST;
            return ERR;
        }
    }

    list_entry_init(&fs->entry);
    list_push(&filesystems.list, &fs->entry);
    return 0;
}

uint64_t vfs_unregister_fs(filesystem_t* fs)
{
    if (fs == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&filesystems.lock);
    RWLOCK_READ_SCOPE(&superblocks.lock);

    superblock_t* superblock;
    LIST_FOR_EACH(superblock, &superblocks.list, entry)
    {
        if (strcmp(superblock->fs->name, fs->name) == 0)
        {
            errno = EBUSY;
            return ERR;
        }
    }

    list_remove(&filesystems.list, &fs->entry);
    return 0;
}

filesystem_t* vfs_get_fs(const char* name)
{
    RWLOCK_READ_SCOPE(&filesystems.lock);

    filesystem_t* fs;
    LIST_FOR_EACH(fs, &filesystems.list, entry)
    {
        if (strcmp(fs->name, name) == 0)
        {
            return fs;
        }
    }

    return NULL;
}

inode_t* vfs_get_inode(superblock_t* superblock, inode_number_t number)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    map_key_t key = inode_cache_key(superblock->id, number);

    RWLOCK_READ_SCOPE(&inodeCache.lock);

    inode_t* inode = CONTAINER_OF_SAFE(map_get(&inodeCache.map, &key), inode_t, mapEntry);
    if (inode == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    if (atomic_load(&inode->ref.count) == 0) // Is currently being removed
    {
        errno = ESTALE;
        return NULL;
    }

    return REF(inode);
}

static dentry_t* vfs_get_dentry_internal(map_key_t* key)
{
    RWLOCK_READ_SCOPE(&dentryCache.lock);

    dentry_t* dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, key), dentry_t, mapEntry);
    if (dentry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    if (atomic_load(&dentry->ref.count) == 0) // Is currently being removed
    {
        errno = ESTALE;
        return NULL;
    }

    return REF(dentry);
}

dentry_t* vfs_get_dentry(const dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->id, name);
    return vfs_get_dentry_internal(&key);
}

dentry_t* vfs_get_or_lookup_dentry(const path_t* parent, const char* name)
{
    if (parent == NULL || parent->dentry == NULL || parent->mount == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->dentry->id, name);
    dentry_t* dentry = vfs_get_dentry_internal(&key);
    if (dentry != NULL)
    {
        return dentry;
    }

    // TODO: The lookup operation needs further verification.

    rwlock_write_acquire(&dentryCache.lock);
    dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, &key), dentry_t, mapEntry);
    if (dentry != NULL) // Check if the the dentry was added while the lock was released above.
    {
        if (atomic_load(&dentry->ref.count) == 0) // Is currently being removed
        {
            rwlock_write_release(&dentryCache.lock);
            errno = ESTALE;
            return NULL;
        }

        dentry = REF(dentry);
        rwlock_write_release(&dentryCache.lock);
        return dentry;
    }

    if (parent->dentry->inode == NULL || parent->dentry->inode->ops == NULL ||
        parent->dentry->inode->ops->lookup == NULL)
    {
        rwlock_write_release(&dentryCache.lock);
        errno = ENOENT;
        return NULL;
    }

    dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        rwlock_write_release(&dentryCache.lock);
        return NULL;
    }
    DEREF_DEFER(dentry);

    if (map_insert(&dentryCache.map, &key, &dentry->mapEntry) == ERR)
    {
        rwlock_write_release(&dentryCache.lock);
        return NULL;
    }

    inode_t* dir = parent->dentry->inode;
    MUTEX_SCOPE(&dir->mutex);
    MUTEX_SCOPE(&dentry->mutex);

    rwlock_write_release(&dentryCache.lock);

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    if (parent->dentry->inode->ops->lookup(dir, dentry) == ERR)
    {
        vfs_remove_dentry(dentry);
        return NULL;
    }

    return REF(dentry);
}

uint64_t vfs_add_inode(inode_t* inode)
{
    map_key_t key = inode_cache_key(inode->superblock->id, inode->number);

    RWLOCK_WRITE_SCOPE(&inodeCache.lock);
    if (map_insert(&inodeCache.map, &key, &inode->mapEntry) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t vfs_add_dentry(dentry_t* dentry)
{
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);

    RWLOCK_WRITE_SCOPE(&dentryCache.lock);
    if (map_insert(&dentryCache.map, &key, &dentry->mapEntry) == ERR)
    {
        return ERR;
    }

    return 0;
}

void vfs_remove_superblock(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&superblocks.lock);
    list_remove(&superblocks.list, &superblock->entry);
}

void vfs_remove_inode(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&inodeCache.lock);
    map_key_t key = inode_cache_key(inode->superblock->id, inode->number);
    map_remove(&inodeCache.map, &key);
}

void vfs_remove_dentry(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&dentryCache.lock);
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);
    map_remove(&dentryCache.map, &key);
}

dentry_t* vfs_get_root_dentry(void)
{
    return REF(root);
}

uint64_t vfs_walk(path_t* outPath, const pathname_t* pathname, walk_flags_t flags, process_t* process)
{
    if (outPath == NULL || !PATHNAME_IS_VALID(pathname) || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t cwd = PATH_EMPTY;
    if (vfs_ctx_get_cwd(&process->vfsCtx, &cwd) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&cwd);

    return path_walk(outPath, pathname, &cwd, flags, &process->namespace);
}

uint64_t vfs_walk_parent(path_t* outPath, const pathname_t* pathname, char* outLastName, walk_flags_t flags,
    process_t* process)
{
    if (outPath == NULL || !PATHNAME_IS_VALID(pathname) || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t cwd = PATH_EMPTY;
    if (vfs_ctx_get_cwd(&process->vfsCtx, &cwd) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&cwd);

    return path_walk_parent(outPath, pathname, &cwd, outLastName, flags, &process->namespace);
}

uint64_t vfs_walk_parent_and_child(path_t* outParent, path_t* outChild, const pathname_t* pathname, walk_flags_t flags,
    process_t* process)
{
    char lastName[MAX_NAME];
    path_t parent = PATH_EMPTY;
    if (vfs_walk_parent(&parent, pathname, lastName, WALK_MOUNTPOINT_TO_ROOT, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parent);

    path_t child = PATH_EMPTY;
    if (path_walk_single_step(&child, &parent, lastName, flags, &process->namespace) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&child);

    path_copy(outParent, &parent);
    path_copy(outChild, &child);
    return 0;
}

bool vfs_is_name_valid(const char* name)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return false;
    }

    for (uint64_t i = 0; i < MAX_NAME - 1; i++)
    {
        if (name[i] == '\0')
        {
            return true;
        }
        if (!PATH_VALID_CHAR(name[i]))
        {
            return false;
        }
    }

    return false;
}

static uint64_t vfs_create(path_t* outPath, const pathname_t* pathname, process_t* process)
{
    char lastComponent[MAX_NAME];
    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (vfs_walk_parent_and_child(&parent, &target, pathname, WALK_NEGATIVE_IS_OK | WALK_MOUNTPOINT_TO_ROOT, process) ==
        ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if (parent.dentry->inode == NULL || parent.dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (parent.dentry->inode->ops == NULL || parent.dentry->inode->ops->create == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&target.dentry->mutex);

    if (!(target.dentry->flags & DENTRY_NEGATIVE))
    {
        if (pathname->flags & PATH_EXCLUSIVE)
        {
            errno = EEXIST;
            return ERR;
        }

        if ((pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
        {
            errno = ENOTDIR;
            return ERR;
        }

        if (!(pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
        {
            errno = EISDIR;
            return ERR;
        }

        path_copy(outPath, &target);
        return 0;
    }

    inode_t* dir = parent.dentry->inode;
    MUTEX_SCOPE(&dir->mutex);

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    if (dir->ops->create(dir, target.dentry, pathname->flags) == ERR)
    {
        return ERR;
    }

    if (target.dentry->flags & DENTRY_NEGATIVE)
    {
        errno = EIO;
        return ERR;
    }

    path_copy(outPath, &target);
    return 0;
}

static uint64_t vfs_open_lookup(path_t* outPath, const pathname_t* pathname, process_t* process)
{
    path_t target = PATH_EMPTY;
    if (pathname->flags & PATH_CREATE)
    {
        if (vfs_create(&target, pathname, process) == ERR)
        {
            return ERR;
        }
    }
    else // Dont create dentry
    {
        if (vfs_walk(&target, pathname, WALK_MOUNTPOINT_TO_ROOT, process) == ERR)
        {
            return ERR;
        }
    }
    PATH_DEFER(&target);

    if (target.dentry->inode == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    if ((pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (!(pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
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

    path_t path = PATH_EMPTY;
    if (vfs_open_lookup(&path, pathname, process) == ERR)
    {
        return NULL;
    }
    PATH_DEFER(&path);

    file_t* file = file_new(path.dentry->inode, &path, pathname->flags);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    if (pathname->flags & PATH_TRUNCATE && path.dentry->inode->type == INODE_FILE)
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

    return vfs_ctx_open(&process->vfsCtx, file);
}

uint64_t vfs_open2(const pathname_t* pathname, file_t* files[2], process_t* process)
{
    if (!PATHNAME_IS_VALID(pathname) || files == NULL || process == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t path = PATH_EMPTY;
    if (vfs_open_lookup(&path, pathname, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    files[0] = file_new(path.dentry->inode, &path, pathname->flags);
    if (files[0] == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(files[0]);

    files[1] = file_new(path.dentry->inode, &path, pathname->flags);
    if (files[1] == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(files[1]);

    if (pathname->flags & PATH_TRUNCATE && path.dentry->inode->type == INODE_FILE)
    {
        inode_truncate(path.dentry->inode);
    }

    if (files[0]->ops == NULL || files[0]->ops->open2 == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = files[0]->ops->open2(files);
    if (result == ERR)
    {
        return ERR;
    }

    inode_notify_access(files[0]->inode);
    REF(files[0]);
    REF(files[1]);
    return 0;
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
    fdsLocal[0] = vfs_ctx_open(&process->vfsCtx, files[0]);
    if (fdsLocal[0] == ERR)
    {
        return ERR;
    }
    fdsLocal[1] = vfs_ctx_open(&process->vfsCtx, files[1]);
    if (fdsLocal[1] == ERR)
    {
        vfs_ctx_close(&process->vfsCtx, fdsLocal[0]);
        return ERR;
    }

    if (thread_copy_to_user(thread, fds, fdsLocal, sizeof(fd_t) * 2) == ERR)
    {
        vfs_ctx_close(&process->vfsCtx, fdsLocal[0]);
        vfs_ctx_close(&process->vfsCtx, fdsLocal[1]);
        return ERR;
    }

    return 0;
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

SYSCALL_DEFINE(SYS_READ, uint64_t, fd_t fd, void* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
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

    if (file->flags & PATH_APPEND && file->ops->seek != NULL && file->ops->seek(file, 0, SEEK_END) == ERR)
    {
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

SYSCALL_DEFINE(SYS_WRITE, uint64_t, fd_t fd, const void* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
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

SYSCALL_DEFINE(SYS_SEEK, uint64_t, fd_t fd, int64_t offset, seek_origin_t origin)
{
    process_t* process = sched_process();

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    return vfs_seek(file, offset, origin);
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

SYSCALL_DEFINE(SYS_IOCTL, uint64_t, fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
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

void* vfs_mmap(file_t* file, void* address, uint64_t length, pml_flags_t flags)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (file->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return NULL;
    }

    if (file->ops == NULL || file->ops->mmap == NULL)
    {
        return NULL;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    void* result = file->ops->mmap(file, address, length, flags);
    if (result != NULL)
    {
        inode_notify_access(file->inode);
    }
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

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    return vfs_mmap(file, address, length, flags | PML_USER);
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

    clock_t uptime = timer_uptime();

    clock_t deadline;
    if (timeout == CLOCKS_NEVER)
    {
        deadline = CLOCKS_NEVER;
    }
    else if (timeout > CLOCKS_NEVER - uptime)
    {
        deadline = CLOCKS_NEVER;
    }
    else
    {
        deadline = uptime + timeout;
    }

    uint64_t readyCount = 0;
    while (true)
    {
        uptime = timer_uptime();
        clock_t remaining = (deadline == CLOCKS_NEVER) ? CLOCKS_NEVER : deadline - uptime;

        if (wait_block_setup(ctx.queues, ctx.queueAmount, remaining) == ERR)
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
        files[i].file = vfs_ctx_get_file(&process->vfsCtx, fds[i].fd);
        if (files[i].file == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                DEREF(files[j].file);
            }
            if (errno == EBADF)
            {
                files[i].revents = POLLNVAL;
            }
            space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);
            return ERR;
        }

        files[i].events = fds[i].events;
        files[i].revents = POLLNONE;
    }
    space_unpin(&process->space, fds, sizeof(pollfd_t) * amount);

    uint64_t result = vfs_poll(files, amount, timeout);
    if (result != ERR)
    {
        for (uint64_t i = 0; i < amount; i++)
        {
            fds[i].revents = files[i].revents;
        }
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        DEREF(files[i].file);
    }

    return result;
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

    mutex_acquire(&file->path.dentry->mutex);
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = file->path.dentry->ops->getdents(file->path.dentry, buffer, count, &file->pos, file->flags);
    mutex_release(&file->path.dentry->mutex);
    if (result != ERR)
    {
        inode_notify_access(file->inode);
    }
    return result;
}

SYSCALL_DEFINE(SYS_GETDENTS, uint64_t, fd_t fd, dirent_t* buffer, uint64_t count)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = vfs_ctx_get_file(&process->vfsCtx, fd);
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

uint64_t vfs_stat(const pathname_t* pathname, stat_t* buffer)
{
    if (!PATHNAME_IS_VALID(pathname) || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (pathname->flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    process_t* process = sched_process();
    assert(process != NULL);

    path_t path = PATH_EMPTY;
    if (vfs_walk(&path, pathname, WALK_NONE, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    memset(buffer, 0, sizeof(stat_t));

    MUTEX_SCOPE(&path.dentry->mutex);
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
    uint64_t result = vfs_stat(&pathname, buffer);
    space_unpin(&process->space, buffer, sizeof(stat_t));
    return result;
}

uint64_t vfs_link(const pathname_t* oldPathname, const pathname_t* newPathname)
{
    if (!PATHNAME_IS_VALID(oldPathname) || !PATHNAME_IS_VALID(newPathname))
    {
        errno = EINVAL;
        return ERR;
    }

    if (oldPathname->flags != PATH_NONE || newPathname->flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    process_t* process = sched_process();
    assert(process != NULL);

    path_t oldParent = PATH_EMPTY;
    path_t old = PATH_EMPTY;
    if (vfs_walk_parent_and_child(&oldParent, &old, oldPathname, WALK_NONE, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&oldParent);
    PATH_DEFER(&old);

    path_t newParent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (vfs_walk_parent_and_child(&newParent, &target, newPathname, WALK_NEGATIVE_IS_OK, process) == ERR)
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

    if (oldParent.dentry->inode == NULL || oldParent.dentry->inode->ops == NULL ||
        oldParent.dentry->inode->ops->link == NULL)
    {

        errno = ENOSYS;
        return ERR;
    }

    mutex_acquire(&old.dentry->inode->mutex);
    mutex_acquire(&newParent.dentry->inode->mutex);

    MUTEX_SCOPE(&target.dentry->mutex);

    uint64_t result = 0;
    if (!(target.dentry->flags & DENTRY_NEGATIVE))
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

    return vfs_link(&oldPathname, &newPathname);
}

uint64_t vfs_delete(const pathname_t* pathname)
{
    if (!PATHNAME_IS_VALID(pathname))
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process = sched_process();
    assert(process != NULL);

    path_t parent = PATH_EMPTY;
    path_t target = PATH_EMPTY;
    if (vfs_walk_parent_and_child(&parent, &target, pathname, WALK_NONE, process) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parent);
    PATH_DEFER(&target);

    if ((pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    if (!(pathname->flags & PATH_DIRECTORY) && target.dentry->inode->type != INODE_FILE)
    {
        errno = EISDIR;
        return ERR;
    }

    inode_t* dir = parent.dentry->inode;
    if (dir->ops == NULL || dir->ops->delete == NULL)
    {
        errno = EPERM;
        return ERR;
    }

    mutex_acquire(&dir->mutex);
    mutex_acquire(&target.dentry->mutex);

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    uint64_t result = dir->ops->delete(dir, target.dentry, pathname->flags);
    if (result != ERR)
    {
        vfs_remove_dentry(target.dentry);
    }

    mutex_release(&target.dentry->mutex);
    mutex_release(&dir->mutex);

    inode_notify_change(target.dentry->inode);

    return result;
}

SYSCALL_DEFINE(SYS_DELETE, uint64_t, const char* pathString)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    pathname_t pathname;
    if (thread_copy_from_user_pathname(thread, &pathname, pathString) == ERR)
    {
        return ERR;
    }

    return vfs_delete(&pathname);
}
