#include "vfs.h"

#include "drivers/systime/systime.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "sync/rwlock.h"
#include "sys/list.h"
#include "sysfs.h"
#include "vfs_ctx.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static _Atomic(uint64_t) newVfsId = ATOMIC_VAR_INIT(0);

static vfs_list_t superblocks;
static vfs_list_t filesystems;

static vfs_map_t dentryCache;
static vfs_map_t inodeCache;
static vfs_map_t mountCache;

static vfs_root_t root;

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

static map_key_t mount_cache_key(mount_id_t parentId, dentry_id_t mountpointId)
{
    uint64_t buffer[2] = {(uint64_t)parentId, (uint64_t)mountpointId};

    return map_key_buffer(buffer, sizeof(buffer));
}

static void vfs_list_init(vfs_list_t* list)
{
    list_init(&list->list);
    rwlock_init(&list->lock);
}

static void vfs_map_init(vfs_map_t* map)
{
    map_init(&map->map);
    rwlock_init(&map->lock);
}

void vfs_init(void)
{
    LOG_INFO("vfs: init\n");

    vfs_list_init(&superblocks);
    vfs_list_init(&filesystems);

    vfs_map_init(&dentryCache);
    vfs_map_init(&inodeCache);
    vfs_map_init(&mountCache);

    root.mount = NULL;
    rwlock_init(&root.lock);

    path_flags_init();
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

    RWLOCK_WRITE_DEFER(&filesystems.lock);

    filesystem_t* existing;
    LIST_FOR_EACH(existing, &filesystems.list, entry)
    {
        if (strcmp(existing->name, fs->name) == 0)
        {
            errno = EEXIST;
            return ERR;
        }
    }

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

    RWLOCK_WRITE_DEFER(&filesystems.lock);
    RWLOCK_READ_DEFER(&superblocks.lock);

    superblock_t* superblock;
    LIST_FOR_EACH(superblock, &superblocks.list, entry)
    {
        if (strcmp(superblock->fs->name, fs->name) == 0)
        {
            errno = EBUSY;
            return ERR;
        }
    }

    list_remove(&fs->entry);
    return 0;
}

filesystem_t* vfs_get_fs(const char* name)
{
    RWLOCK_READ_DEFER(&filesystems.lock);

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

uint64_t vfs_get_global_root(path_t* outPath)
{
    rwlock_read_acquire(&root.lock);

    if (root.mount == NULL || root.mount->superblock->root == NULL)
    {
        rwlock_read_release(&root.lock);
        errno = ENOENT;
        return ERR;
    }

    outPath->mount = mount_ref(root.mount);
    outPath->dentry = dentry_ref(root.mount->superblock->root);

    rwlock_read_release(&root.lock);
    return 0;
}

uint64_t vfs_mountpoint_to_mount_root(path_t* outRoot, const path_t* mountpoint)
{
    map_key_t mountKey = mount_cache_key(mountpoint->mount->id, mountpoint->dentry->id);

    rwlock_read_acquire(&mountCache.lock);
    mount_t* mount = CONTAINER_OF_SAFE(map_get(&mountCache.map, &mountKey), mount_t, mapEntry);
    if (mount == NULL)
    {
        rwlock_read_release(&mountCache.lock);
        errno = ENOENT;
        return ERR;
    }

    if (mount->superblock == NULL || mount->superblock->root == NULL)
    {
        rwlock_read_release(&mountCache.lock);
        errno = ESTALE;
        return ERR;
    }

    outRoot->mount = mount_ref(mount);
    outRoot->dentry = dentry_ref(mount->superblock->root);
    rwlock_read_release(&mountCache.lock);
    return 0;
}

inode_t* vfs_get_inode(superblock_t* superblock, inode_number_t number)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    map_key_t key = inode_cache_key(superblock->id, number);

    rwlock_read_acquire(&inodeCache.lock);
    inode_t* inode = CONTAINER_OF_SAFE(map_get(&inodeCache.map, &key), inode_t, mapEntry);
    if (inode != NULL)
    {
        inode = inode_ref(inode);
        rwlock_read_release(&inodeCache.lock);
        return inode;
    }
    rwlock_read_release(&inodeCache.lock);

    return 0;
}

static dentry_t* vfs_get_dentry_internal(map_key_t* key)
{
    rwlock_read_acquire(&dentryCache.lock);
    dentry_t* dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, key), dentry_t, mapEntry);
    if (dentry == NULL)
    {
        rwlock_read_release(&dentryCache.lock);
        return NULL;
    }
    dentry = dentry_ref(dentry);
    rwlock_read_release(&dentryCache.lock);
    DENTRY_DEFER(dentry);

    LOCK_DEFER(&dentry->lock);

    if (WAIT_BLOCK_LOCK(&dentry->lookupWaitQueue, &dentry->lock, !(dentry->flags & DENTRY_LOOKUP_PENDING)) != WAIT_NORM)
    {
        errno = EINTR;
        return NULL;
    }
        
    if (dentry->flags & DENTRY_NEGATIVE) 
    {
        errno = ENOENT;
        return NULL;
    } 

    return dentry_ref(dentry);
}

dentry_t* vfs_get_dentry(dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->id, name);
    return vfs_get_dentry_internal(&key);
}

dentry_t* vfs_get_or_lookup_dentry(dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->id, name);

    dentry_t* dentry = vfs_get_dentry_internal(&key);
    if (dentry != NULL)
    {
        return dentry;
    }

    // Dentry was not found.

    rwlock_write_acquire(&dentryCache.lock);    
    dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, &key), dentry_t, mapEntry);
    if (dentry != NULL) // Check if the the dentry was added while the lock was released above.
    {
        dentry = dentry_ref(dentry);
        rwlock_write_release(&dentryCache.lock);
        DENTRY_DEFER(dentry);

        LOCK_DEFER(&dentry->lock);

        if (WAIT_BLOCK_LOCK(&dentry->lookupWaitQueue, &dentry->lock, !(dentry->flags & DENTRY_LOOKUP_PENDING)) != WAIT_NORM)
        {
            errno = EINTR;
            return NULL;
        }
            
        if (dentry->flags & DENTRY_NEGATIVE) 
        {
            errno = ENOENT;
            return NULL;
        } 

        return dentry_ref(dentry);
    }

    if (parent->inode == NULL || parent->inode->ops == NULL || parent->inode->ops->lookup == NULL)
    {
        rwlock_write_release(&dentryCache.lock);
        errno = ENOSYS;
        return NULL;
    }

    dentry = dentry_new(parent->superblock, parent, name);
    if (dentry == NULL)
    {
        rwlock_write_release(&dentryCache.lock);
        return NULL;
    }
    DENTRY_DEFER(dentry);
    
    if (map_insert(&dentryCache.map, &key, &dentry->mapEntry) == ERR)
    {
        rwlock_write_release(&dentryCache.lock);
        return NULL;
    }

    rwlock_write_release(&dentryCache.lock);

    lookup_result_t result = parent->inode->ops->lookup(parent->inode, dentry);
    switch (result)
    {
    case LOOKUP_FOUND:
        return dentry_ref(dentry);
    case LOOKUP_NO_ENTRY:
        dentry_make_positive(dentry, NULL); // No longer pending but still negative.
        return dentry_ref(dentry);
    case LOOKUP_ERROR:
        return NULL;
    default:
        errno = EIO;
        return NULL;
    }
}

uint64_t vfs_add_dentry(dentry_t* dentry)
{    
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);

    RWLOCK_WRITE_DEFER(&dentryCache.lock);
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

    rwlock_write_acquire(&superblocks.lock);
    list_remove(&superblock->entry);
    rwlock_write_release(&superblocks.lock);
}

void vfs_remove_inode(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    rwlock_write_acquire(&inodeCache.lock);
    map_key_t key = inode_cache_key(inode->superblock->id, inode->number);
    map_remove(&inodeCache.map, &key);
    rwlock_write_release(&inodeCache.lock);
}

void vfs_remove_dentry(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    rwlock_write_acquire(&dentryCache.lock);
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);
    map_remove(&dentryCache.map, &key);
    rwlock_write_release(&dentryCache.lock);
}

uint64_t vfs_walk(path_t* outPath, const char* pathname)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return path_walk(outPath, pathname, &cwd);
}

uint64_t vfs_walk_parent(path_t* outPath, const char* pathname, char* outLastName)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return path_walk_parent(outPath, pathname, &cwd, outLastName);
}

uint64_t vfs_mount(const char* deviceName, const char* mountpoint, const char* fsName, superblock_flags_t flags,
    void* private)
{
    if (deviceName == NULL || mountpoint == NULL || fsName == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    filesystem_t* fs = vfs_get_fs(fsName);
    if (fs == NULL)
    {
        errno = ENODEV;
        return ERR;
    }

    dentry_t* dentry = fs->mount(fs, flags, deviceName, private);
    if (dentry == NULL)
    {
        return ERR;
    }
    DENTRY_DEFER(dentry);

    lock_acquire(&dentry->lock);
    if (dentry->flags & DENTRY_NEGATIVE)
    {
        lock_release(&dentry->lock);
        errno = EIO;
        return ERR;
    }
    lock_release(&dentry->lock);

    if (root.mount == NULL) // Special handling for mounting inital file system, since this can only happen during boot
                            // there is no need for the lock before the if statement.
    {
        assert(strcmp(mountpoint, "/") == 0); // Sanity check.

        rwlock_write_acquire(&root.lock);
        root.mount = mount_new(dentry->superblock, NULL);
        assert(root.mount != NULL); // Only happens during boot, must not fail.
        rwlock_write_release(&root.lock);
        return 0;
    }

    path_t mountPath;
    if (vfs_walk(&mountPath, mountpoint) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&mountPath);

    LOCK_DEFER(&mountPath.dentry->lock);

    if (mountPath.dentry->flags & DENTRY_MOUNTPOINT)
    {
        errno = EBUSY;
        return ERR;
    }

    if (mountPath.dentry->flags & DENTRY_NEGATIVE)
    {
        errno = ENOENT;
        return ERR;
    }

    mount_t* mount = mount_new(dentry->superblock, &mountPath);
    if (mount == NULL)
    {
        return ERR;
    }

    mountPath.dentry->flags |= DENTRY_MOUNTPOINT;

    map_key_t key = mount_cache_key(mountPath.mount->id, mountPath.dentry->id);
    rwlock_write_acquire(&mountCache.lock);
    if (map_insert(&mountCache.map, &key, &mount_ref(mount)->mapEntry) == ERR)
    {
        mount_deref(mount);
        rwlock_write_release(&mountCache.lock);
        return ERR;
    }
    rwlock_write_release(&mountCache.lock);

    // superblock_expose(superblock); // TODO: Expose the sysdir for the superblock

    LOG_INFO("vfs: mounted %s on %s (type %s)\n", deviceName, mountpoint, fsName);
    return 0;
}

uint64_t vfs_unmount(const char* mountpoint)
{
    if (mountpoint == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t path;
    if (vfs_walk(&path, mountpoint) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    RWLOCK_READ_DEFER(&root.lock);
    RWLOCK_WRITE_DEFER(&mountCache.lock);
    LOCK_DEFER(&path.dentry->lock);

    if (!(path.dentry->flags & DENTRY_MOUNTPOINT))
    {
        errno = EINVAL;
        return ERR;
    }

    map_key_t key = mount_cache_key(path.mount->id, path.dentry->id);
    mount_t* mount = CONTAINER_OF_SAFE(map_get(&mountCache.map, &key), mount_t, mapEntry);
    if (mount == NULL)
    {
        return ERR;
    }
    mount = mount_ref(mount);
    MOUNT_DEFER(mount);

    if (mount == root.mount)
    {
        errno = EBUSY;
        return ERR;
    }

    if (atomic_load(&mount->superblock->ref) > 1)
    {
        errno = EBUSY;
        return ERR;
    }

    map_remove(&mountCache.map, &key);
    mount_deref(mount); // Remove reference in cache.

    path.dentry->flags &= ~DENTRY_MOUNTPOINT;

    LOG_INFO("vfs: unmounted %s\n", mountpoint);
    return 0;
}

bool vfs_is_name_valid(const char* name)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return false;
    }

    for (uint64_t i = 0; i < MAX_NAME; i++)
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

static dentry_t* vfs_open_lookup(const char* pathname, path_flags_t* outFlags)
{
    parsed_pathname_t parsed;
    if (path_parse_pathname(&parsed, pathname) == ERR)
    {
        return NULL;
    }

    dentry_t* dentry = NULL;
    if (parsed.flags & PATH_CREATE)
    {
        char lastComponent[MAX_NAME];
        path_t parent;
        if (vfs_walk_parent(&parent, parsed.pathname, lastComponent) == ERR)
        {
            return NULL;
        }
        PATH_DEFER(&parent);

        if (parent.dentry->inode == NULL || parent.dentry->inode->type != INODE_DIR)
        {
            errno = ENOTDIR;
            return NULL;
        }
        
        if (parent.dentry->inode->ops == NULL || parent.dentry->inode->ops->create == NULL)
        {
            errno = ENOSYS;
            return NULL;
        }

        // If the file/dir does not exist it will return a negative dentry.
        dentry = vfs_get_or_lookup_dentry(parent.dentry, lastComponent);
        if (dentry == NULL)
        {
            return NULL;
        }
        DENTRY_DEFER(dentry);

        if (parent.dentry->inode->ops->create(parent.dentry->inode, dentry, parsed.flags) == ERR)
        {
            return NULL;
        }

        if (dentry->flags & DENTRY_NEGATIVE)
        {
            errno = EIO; 
            return NULL;
        }
        
        dentry = dentry_ref(dentry);
    }
    else // Dont create dentry
    {
        path_t path;
        if (vfs_walk(&path, parsed.pathname) == ERR)
        {
            return NULL;
        }

        dentry = dentry_ref(path.dentry);
        path_put(&path);
    }
    DENTRY_DEFER(dentry);
    
    if (dentry->inode == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    if ((parsed.flags & PATH_DIRECTORY) && dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return NULL;
    }

    if (!(parsed.flags & PATH_DIRECTORY) && dentry->inode->type != INODE_FILE)
    {
        errno = EISDIR;
        return NULL;
    }

    *outFlags = parsed.flags;
    return dentry_ref(dentry);
}

file_t* vfs_open(const char* pathname)
{
    if (pathname == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    path_flags_t flags;
    dentry_t* dentry = vfs_open_lookup(pathname, &flags);
    if (dentry == NULL)
    {
        return NULL;
    }
    DENTRY_DEFER(dentry);

    file_t* file = file_new(dentry, flags);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_DEFER(file);

    if ((flags & PATH_TRUNCATE) && dentry->inode->type == INODE_FILE)
    {
        if (dentry->inode->ops == NULL || dentry->inode->ops->truncate == NULL)
        {
            errno = ENOSYS;
            return NULL;
        }

        dentry->inode->ops->truncate(dentry->inode);
    }

    if (file->ops != NULL && file->ops->open != NULL)
    {
        uint64_t result = file->ops->open(file);
        if (result == ERR)
        {
            return NULL;
        }
    }

    return file_ref(file);
}

uint64_t vfs_open2(const char* pathname, file_t* files[2])
{
    if (pathname == NULL || files == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_flags_t flags;
    dentry_t* dentry = vfs_open_lookup(pathname, &flags);
    if (dentry == NULL)
    {
        return ERR;
    }
    DENTRY_DEFER(dentry);

    files[0] = file_new(dentry, flags);
    if (files[0] == NULL)
    {
        return ERR;
    }
    FILE_DEFER(files[0]);

    files[1] = file_new(dentry, flags);
    if (files[1] == NULL)
    {
        return ERR;
    }
    FILE_DEFER(files[1]);

    if ((flags & PATH_TRUNCATE) && dentry->inode->type == INODE_FILE)
    {
        if (dentry->inode->ops == NULL || dentry->inode->ops->truncate == NULL)
        {
            errno = ENOSYS;
            return ERR;
        }

        dentry->inode->ops->truncate(dentry->inode);
    }

    if (files[0]->ops == NULL || files[0]->ops->open2 == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    uint64_t result = files[0]->ops->open2(files);
    if (result == ERR)
    {
        return ERR;
    }

    file_ref(files[0]);
    file_ref(files[1]);
    return 0;
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file == NULL || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->dentry->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->read == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    uint64_t offset = file->pos;
    uint64_t result = file->ops->read(file, buffer, count, &offset);
    file->pos = offset;
    if (result != ERR)
    {
        inode_t* inode = file->dentry->inode;

        LOCK_DEFER(&inode->lock);
        inode->accessTime = systime_unix_epoch();
        inode->flags |= INODE_DIRTY;
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

    if (file->dentry->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->write == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    uint64_t offset = file->pos;
    uint64_t result = file->ops->write(file, buffer, count, &offset);
    file->pos = offset;
    if (result != ERR)
    {
        inode_t* inode = file->dentry->inode;

        LOCK_DEFER(&inode->lock);
        inode->modifyTime = systime_unix_epoch();
        inode->changeTime = inode->modifyTime;
        inode->flags |= INODE_DIRTY;
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

    if (file->dentry->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops != NULL && file->ops->seek != NULL)
    {
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

    if (file->dentry->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return ERR;
    }

    if (file->ops == NULL || file->ops->ioctl == NULL)
    {
        errno = ENOTTY;
        return ERR;
    }

    return file->ops->ioctl(file, request, argp, size);
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (file->dentry->inode->type == INODE_DIR)
    {
        errno = EISDIR;
        return NULL;
    }

    if (file->ops == NULL || file->ops->mmap == NULL)
    {
        return NULL;
    }

    return file->ops->mmap(file, address, length, prot);
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout)
{
    if (files == NULL || amount == 0)
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
        if (files[i].file->dentry->inode->type == INODE_DIR)
        {
            errno = EISDIR;
            return ERR;
        }
        if (files[i].file->ops == NULL || files[i].file->ops->poll == NULL)
        {
            errno = ENOSYS;
            return ERR;
        }
        files[i].occoured = POLL_NONE;
    }

    wait_queue_t** waitQueues = heap_alloc(sizeof(wait_queue_t*) * amount, HEAP_VMM);
    if (waitQueues == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        waitQueues[i] = NULL;
    }

    clock_t uptime = systime_uptime();
    clock_t deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : uptime + timeout;

    uint64_t readyCount = 0;
    while (true)
    {
        readyCount = 0;

        for (uint64_t i = 0; i < amount; i++)
        {
            waitQueues[i] = files[i].file->ops->poll(files[i].file, &files[i]);
            if (waitQueues[i] == NULL)
            {
                heap_free(waitQueues);
                return ERR;
            }

            if ((files[i].occoured & files[i].events) != 0)
            {
                readyCount++;
            }
        }

        if (readyCount > 0)
        {
            break;
        }

        uptime = systime_uptime();
        if (uptime >= deadline)
        {
            break;
        }

        clock_t remaining = deadline == CLOCKS_NEVER ? CLOCKS_NEVER : deadline - uptime;
        wait_block_many(waitQueues, amount, remaining);
        uptime = systime_uptime();
    }

    heap_free(waitQueues);
    return readyCount;
}

uint64_t vfs_getdirent(file_t* file, dirent_t* buffer, uint64_t amount)
{
    if (file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (buffer == NULL && amount != 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->dentry == NULL || file->dentry->inode == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (file->dentry->inode->type != INODE_DIR)
    {
        errno = ENOTDIR;
        return ERR;
    }

    LOCK_DEFER(&file->dentry->lock);
    LOCK_DEFER(&file->dentry->inode->lock);

    getdirent_ctx_t ctx = {0};

    getdirent_write(&ctx, buffer, amount, file->dentry->inode->number, file->dentry->inode->type, ".");

    if (file->dentry->parent != NULL && file->dentry->parent->inode != NULL) // Will still add ".." entry if dentry is root.
    {
        if (file->dentry == file->dentry->parent)
        {
            getdirent_write(&ctx, buffer, amount, file->dentry->inode->number, file->dentry->inode->type, "..");
        }
        else
        {
            LOCK_DEFER(&file->dentry->parent->lock);
            LOCK_DEFER(&file->dentry->parent->inode->lock);
            getdirent_write(&ctx, buffer, amount, file->dentry->parent->inode->number, file->dentry->parent->inode->type, "..");
        }
    }

    dentry_t* child;
    LIST_FOR_EACH(child, &file->dentry->children, siblingEntry)
    {
        LOCK_DEFER(&child->lock);
        LOCK_DEFER(&child->inode->lock);
        getdirent_write(&ctx, buffer, amount, child->inode->number, child->inode->type, child->name);
    }

    return ctx.total;
}

uint64_t vfs_stat(const char* pathname, stat_t* buffer)
{
    if (pathname == NULL || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    parsed_pathname_t parsed;
    if (path_parse_pathname(&parsed, pathname) == ERR)
    {
        return ERR;
    }

    if (parsed.flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    path_t path;
    if (vfs_walk(&path, parsed.pathname) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    memset(buffer, 0, sizeof(stat_t));

    LOCK_DEFER(&path.dentry->lock);
    LOCK_DEFER(&path.dentry->inode->lock);

    buffer->number = path.dentry->inode->number;
    buffer->type = path.dentry->inode->type;
    buffer->size = path.dentry->inode->size;
    buffer->blocks = path.dentry->inode->blocks;
    buffer->linkAmount = path.dentry->inode->linkCount;
    buffer->accessTime = path.dentry->inode->accessTime;
    buffer->modifyTime = path.dentry->inode->modifyTime;
    buffer->changeTime = path.dentry->inode->changeTime;
    strncpy(buffer->name, path.dentry->name, MAX_NAME - 1);
    buffer->name[MAX_NAME - 1] = '\0';

    return 0;
}

uint64_t vfs_link(const char* oldPathname, const char* newPathname)
{
    /*if (oldPathname == NULL || newPathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    parsed_pathname_t oldParsed;
    if (path_parse_pathname(&oldParsed, oldPathname) == ERR)
    {
        return ERR;
    }

    parsed_pathname_t newParsed;
    if (path_parse_pathname(&newParsed, newPathname) == ERR)
    {
        return ERR;
    }

    if (oldParsed.flags != PATH_NONE || newParsed.flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    path_t oldPath;
    if (vfs_walk(&oldPath, oldParsed.pathname) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&oldPath);

    if (oldPath.dentry->inode->type == INODE_DIR)
    {
        errno = EPERM;
        return ERR;
    }

    char newLastName[MAX_NAME];
    path_t newParentPath;
    if (vfs_walk_parent(&newParentPath, newParsed.pathname, newLastName) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&newParentPath);

    if (!vfs_is_name_valid(newLastName))
    {
        errno = EINVAL;
        return ERR;
    }

    if (oldPath.dentry->superblock->id != newParentPath.dentry->superblock->id)
    {
        errno = EXDEV;
        return ERR;
    }

    if (newParentPath.dentry->inode == NULL || newParentPath.dentry->inode->ops == NULL ||
        newParentPath.dentry->inode->ops->link == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    inode_t* newInode =
        newParentPath.dentry->inode->ops->link(oldPath.dentry, newParentPath.dentry->inode, newLastName);
    if (newInode == NULL)
    {
        return ERR;
    }
    INODE_DEFER(newInode);

    dentry_t* newDentry = dentry_new(oldPath.dentry->superblock, newLastName, newInode);
    if (newDentry == NULL)
    {
        return ERR;
    }
    newDentry->parent = dentry_ref(newParentPath.dentry);

    map_key_t newDentryKey = dentry_cache_key(newParentPath.dentry->id, newLastName);
    rwlock_write_acquire(&dentryCache.lock);
    if (map_insert(&dentryCache.map, &newDentryKey, &newDentry->mapEntry) == ERR)
    {
        dentry_deref(newDentry);
        rwlock_write_release(&dentryCache.lock);
        return ERR;
    }
    rwlock_write_release(&dentryCache.lock);

    lock_acquire(&oldPath.dentry->inode->lock);
    oldPath.dentry->inode->changeTime = systime_unix_epoch();
    oldPath.dentry->inode->flags |= INODE_DIRTY;
    lock_release(&oldPath.dentry->inode->lock);

    lock_acquire(&newParentPath.dentry->inode->lock);
    newParentPath.dentry->inode->modifyTime = systime_unix_epoch();
    newParentPath.dentry->inode->changeTime = newParentPath.dentry->inode->modifyTime;
    newParentPath.dentry->inode->flags |= INODE_DIRTY;
    lock_release(&newParentPath.dentry->inode->lock);*/

    return 0;
}

uint64_t vfs_rename(const char* oldPathname, const char* newPathname)
{
    /*if (oldPathname == NULL || newPathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    parsed_pathname_t oldParsed;
    if (path_parse_pathname(&oldParsed, oldPathname) == ERR)
    {
        return ERR;
    }

    parsed_pathname_t newParsed;
    if (path_parse_pathname(&newParsed, newPathname) == ERR)
    {
        return ERR;
    }

    if (oldParsed.flags != PATH_NONE || newParsed.flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    char oldLastName[MAX_NAME];
    path_t oldParentPath;
    if (vfs_walk_parent(&oldParentPath, oldParsed.pathname, oldLastName) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&oldParentPath);

    dentry_t* oldDentry = vfs_get_or_lookup_dentry(oldParentPath.dentry, oldLastName);
    if (oldDentry == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DENTRY_DEFER(oldDentry);

    char newLastName[MAX_NAME];
    path_t newParentPath;
    if (vfs_walk_parent(&newParentPath, newParsed.pathname, newLastName) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&newParentPath);

    if (!vfs_is_name_valid(newLastName))
    {
        errno = EINVAL;
        return ERR;
    }

    if (oldParentPath.dentry->superblock->id != newParentPath.dentry->superblock->id)
    {
        errno = EXDEV;
        return ERR;
    }

    dentry_t* existingDentry = vfs_get_or_lookup_dentry(newParentPath.dentry, newLastName);
    if (existingDentry != NULL)
    {
        DENTRY_DEFER(existingDentry);

        if (oldDentry->inode->type == INODE_FILE && existingDentry->inode->type == INODE_DIR) {
            errno = EISDIR;
            return ERR;
        }
        if (oldDentry->inode->type == INODE_DIR && existingDentry->inode->type == INODE_FILE) {
            errno = ENOTDIR;
            return ERR;
        }
    }

    if (oldParentPath.dentry->inode == NULL || oldParentPath.dentry->inode->ops == NULL ||
        oldParentPath.dentry->inode->ops->rename == NULL) {

        errno = ENOSYS;
        return ERR;
    }

    inode_t* newInode = oldParentPath.dentry->inode->ops->rename(oldParentPath.dentry->inode, oldDentry, newParentPath.dentry->inode, newLastName);
    if (newInode == NULL)
    {
        return ERR;
    }
    INODE_DEFER(newInode);

    dentry_t* newDentry = dentry_new(oldParentPath.dentry->superblock, newLastName, newInode);
    if (newDentry == NULL)
    {
        return ERR;
    }
    newDentry->parent = dentry_ref(newParentPath.dentry);

    map_key_t oldDentryKey = dentry_cache_key(oldParentPath.dentry->id, oldLastName);
    map_key_t newDentryKey = dentry_cache_key(newParentPath.dentry->id, newLastName);

    rwlock_write_acquire(&dentryCache.lock);
    map_remove(&dentryCache.map, &oldDentryKey);
    if (map_insert(&dentryCache.map, &newDentryKey, &newDentry->mapEntry) == ERR)
    {
        dentry_deref(newDentry);
        rwlock_write_release(&dentryCache.lock);
        return ERR;
    }
    rwlock_write_release(&dentryCache.lock);

    lock_acquire(&oldParentPath.dentry->inode->lock);
    oldParentPath.dentry->inode->modifyTime = systime_unix_epoch();
    oldParentPath.dentry->inode->changeTime = oldParentPath.dentry->inode->modifyTime;
    oldParentPath.dentry->inode->flags |= INODE_DIRTY;
    lock_release(&oldParentPath.dentry->inode->lock);

    if (newParentPath.dentry->inode->number != oldParentPath.dentry->inode->number ||
        newParentPath.dentry->superblock->id != oldParentPath.dentry->superblock->id)
    {
        LOCK_DEFER(&newParentPath.dentry->inode->lock);
        newParentPath.dentry->inode->modifyTime = systime_unix_epoch();
        newParentPath.dentry->inode->changeTime = newParentPath.dentry->inode->modifyTime;
        newParentPath.dentry->inode->flags |= INODE_DIRTY;
    }

    lock_acquire(&oldDentry->inode->lock);
    oldDentry->inode->changeTime = systime_unix_epoch();
    oldDentry->inode->flags |= INODE_DIRTY;
    lock_release(&oldDentry->inode->lock);*/

    return 0;
}

uint64_t vfs_remove(const char* pathname)
{
    /*if (pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    parsed_pathname_t parsed;
    if (path_parse_pathname(&parsed, pathname) == ERR)
    {
        return ERR;
    }

    if (parsed.flags != PATH_NONE)
    {
        errno = EBADFLAG;
        return ERR;
    }

    char lastName[MAX_NAME];
    path_t parentPath;
    if (vfs_walk_parent(&parentPath, parsed.pathname, lastName) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&parentPath);

    dentry_t* target = vfs_get_or_lookup_dentry(parentPath.dentry, lastName);
    if (target == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DENTRY_DEFER(target);

    if (parentPath.dentry->inode == NULL || parentPath.dentry->inode->ops == NULL ||
        parentPath.dentry->inode->ops->remove == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    uint64_t result = parentPath.dentry->inode->ops->remove(parentPath.dentry->inode, target);
    if (result == ERR)
    {
        return ERR;
    }

    map_key_t key = dentry_cache_key(parentPath.dentry->id, lastName);
    rwlock_write_acquire(&dentryCache.lock);
    map_remove(&dentryCache.map, &key);
    rwlock_write_release(&dentryCache.lock);

    lock_acquire(&parentPath.dentry->inode->lock);
    parentPath.dentry->inode->modifyTime = systime_unix_epoch();
    parentPath.dentry->inode->changeTime = parentPath.dentry->inode->modifyTime;
    parentPath.dentry->inode->flags |= INODE_DIRTY;
    lock_release(&parentPath.dentry->inode->lock);

    lock_acquire(&target->inode->lock);
    target->inode->changeTime = systime_unix_epoch();
    target->inode->flags |= INODE_DIRTY;
    lock_release(&target->inode->lock);*/

    return 0;
}

void getdirent_write(getdirent_ctx_t* ctx, dirent_t* buffer, uint64_t amount, inode_number_t number, inode_type_t type,
    const char* name)
{
    if (ctx->index < amount)
    {
        dirent_t* dirent = &buffer[ctx->index++];
        dirent->number = number;
        dirent->type = type;
        strncpy(dirent->name, name, MAX_NAME - 1);
        dirent->name[MAX_NAME - 1] = '\0';
    }

    ctx->total++;
};