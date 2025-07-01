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

static map_key_t inode_key(superblock_id_t superblockId, inode_number_t number)
{
    uint64_t buffer[2] = {(uint64_t)superblockId, (uint64_t)number};

    return map_key_buffer(buffer, sizeof(buffer));
}

static map_key_t dentry_key(dentry_id_t parentId, const char* name)
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

static map_key_t mount_key(mount_id_t parentId, dentry_id_t mountpointId)
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
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&filesystems.lock);

    filesystem_t* existing;
    LIST_FOR_EACH(existing, &filesystems.list, entry)
    {
        if (strcmp(existing->name, fs->name) == 0)
        {
            return ERROR(EEXIST);
        }
    }

    list_push(&filesystems.list, &fs->entry);
    return 0;
}

uint64_t vfs_unregister_fs(filesystem_t* fs)
{
    if (fs == NULL)
    {
        return ERROR(EINVAL);
    }

    RWLOCK_WRITE_DEFER(&filesystems.lock);
    RWLOCK_READ_DEFER(&superblocks.lock);

    superblock_t* superblock;
    LIST_FOR_EACH(superblock, &superblocks.list, entry)
    {
        if (strcmp(superblock->fsName, fs->name) == 0)
        {
            return ERROR(EBUSY);
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

    LOG_INFO("root.mount: %p\n", root.mount);
    LOG_INFO("root.mount->superblock: %p\n", root.mount->superblock);
    LOG_INFO("root.mount->superblock->root: %p\n", root.mount->superblock->root);
    if (root.mount == NULL || root.mount->superblock->root == NULL)
    {
        rwlock_read_release(&root.lock);
        return ERROR(ENOENT);
    }

    outPath->mount = mount_ref(root.mount);
    outPath->dentry = dentry_ref(root.mount->superblock->root);

    rwlock_read_release(&root.lock);
    return 0;
}

uint64_t vfs_mountpoint_to_mount_root(path_t* outRoot, const path_t* mountpoint)
{
    map_key_t mountKey = mount_key(mountpoint->mount->id, mountpoint->dentry->id);

    rwlock_read_acquire(&mountCache.lock);
    mount_t* foundMount = CONTAINER_OF_SAFE(map_get(&mountCache.map, &mountKey), mount_t, mapEntry);
    if (foundMount == NULL)
    {
        rwlock_read_release(&mountCache.lock);
        return ERROR(ENOENT);
    }

    if (foundMount->superblock == NULL || foundMount->superblock->root == NULL)
    {
        rwlock_read_release(&mountCache.lock);
        return ERROR(ESTALE);
    }

    outRoot->mount = mount_ref(foundMount);
    outRoot->dentry = dentry_ref(foundMount->superblock->root);
    rwlock_read_release(&mountCache.lock);
    return 0;
}

inode_t* vfs_get_inode(superblock_t* superblock, inode_number_t number)
{
    return ERRPTR(ENOSYS);
    /*
    if (superblock == NULL)
    {
        return NULL;
    }

    rwlock_read_acquire(&inodeCache.lock);

    map_key_t key = inode_key(superblock->id, id);
    inode_t* inode = CONTAINER_OF_SAFE(map_get(&inodeCache.map, &key), inode_t, mapEntry);
    if (inode != NULL)
    {
        inode = inode_ref(inode);
        rwlock_read_release(&inodeCache.lock);
        return inode;
    }
    rwlock_read_release(&inodeCache.lock);

    rwlock_write_acquire(&inodeCache.lock);
    inode_t* inode = CONTAINER_OF_SAFE(map_get(&inodeCache.map, &key), inode_t, mapEntry);
    if (inode != NULL) // Check if the the inode was added while the lock was released above.
    {
        inode = inode_ref(inode);
        rwlock_write_release(&inodeCache.lock);
        return inode;
    }

    inode = inode_new(superblock); // TODO: How to pass arguments for creating a new inode?
    if (inode == NULL)
    {
        rwlock_write_release(&inodeCache.lock);
        return NULL;
    }

    inode->id = id;
    map_insert(&inodeCache.map, &key, &inode->mapEntry);
    rwlock_write_release(&inodeCache.lock);
    return inode; // This is the inital reference.
    */
}

dentry_t* vfs_get_dentry(dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        return ERRPTR(EINVAL);
    }

    map_key_t key = dentry_key(parent->id, name);

    rwlock_read_acquire(&dentryCache.lock);
    dentry_t* dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, &key), dentry_t, mapEntry);
    if (dentry != NULL)
    {
        dentry = dentry_ref(dentry);
        rwlock_read_release(&dentryCache.lock);
        return dentry;
    }

    rwlock_read_release(&dentryCache.lock);

    rwlock_write_acquire(&dentryCache.lock);
    dentry = CONTAINER_OF_SAFE(map_get(&dentryCache.map, &key), dentry_t, mapEntry);
    if (dentry != NULL) // Check if the the dentry was added while the lock was released above.
    {
        dentry = dentry_ref(dentry);
        rwlock_write_release(&dentryCache.lock);
        return dentry;
    }

    if (parent->inode != NULL && parent->inode->ops != NULL && parent->inode->ops->lookup != NULL)
    {
        dentry = parent->inode->ops->lookup(parent->inode, name);
        if (dentry != NULL)
        {
            dentry->parent = dentry_ref(parent);
            map_insert(&dentryCache.map, &key, &dentry->mapEntry);
        }
    }
    else
    {
        dentry = NULL;
    }

    rwlock_write_release(&dentryCache.lock);
    return dentry;
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
    map_key_t key = inode_key(inode->superblock->id, inode->number);
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
    map_key_t key = dentry_key(dentry->parent->id, dentry->name);
    map_remove(&dentryCache.map, &key);
    rwlock_write_release(&dentryCache.lock);
}

uint64_t vfs_lookup(path_t* outPath, const char* pathname)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return path_walk(outPath, pathname, &cwd);
}

uint64_t vfs_lookup_parent(path_t* outPath, const char* pathname, char* outLastName)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return path_walk_parent(outPath, pathname, &cwd, outLastName);
}

uint64_t vfs_mount(const char* deviceName, const char* mountpoint, const char* fsName, superblock_flags_t flags,
    const void* data)
{
    if (deviceName == NULL || mountpoint == NULL || fsName == NULL)
    {
        return ERROR(EINVAL);
    }

    filesystem_t* fs = vfs_get_fs(fsName);
    if (fs == NULL)
    {
        return ERROR(ENODEV);
    }

    superblock_t* superblock = fs->mount(deviceName, flags, data);
    if (superblock == NULL)
    {
        return ERR;
    }
    SUPER_DEFER(superblock);

    if (root.mount == NULL) // Special handling for mounting inital file system, since this can only happen during boot
                            // there is no need for the lock before the if statement.§
    {
        assert(strcmp(mountpoint, "/") == 0); // Sanity check.

        rwlock_write_acquire(&root.lock);
        root.mount = mount_new(superblock, NULL);
        assert(root.mount != NULL); // Only happens during boot, must not fail.
        rwlock_write_release(&root.lock);
        return 0;
    }

    path_t path;
    if (vfs_lookup(&path, mountpoint) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    lock_acquire(&path.dentry->lock);
    if (path.dentry->flags & DENTRY_MOUNTPOINT)
    {
        lock_release(&path.dentry->lock);
        return ERROR(EBUSY);
    }

    mount_t* mount = mount_new(superblock, &path);
    if (mount == NULL)
    {
        lock_release(&path.dentry->lock);
        return ERR;
    }

    path.dentry->flags |= DENTRY_MOUNTPOINT;

    map_key_t key = mount_key(path.mount->id, path.dentry->id);
    rwlock_write_acquire(&mountCache.lock);
    map_insert(&mountCache.map, &key, &mount_ref(mount)->mapEntry);
    rwlock_write_release(&mountCache.lock);
    lock_release(&path.dentry->lock);

    // superblock_expose(superblock); // TODO: Expose the sysdir for the superblock

    LOG_INFO("vfs: mounted %s on %s (type %s)\n", deviceName, mountpoint, fsName);
    return 0;
}

uint64_t vfs_unmount(const char* mountpoint)
{
    if (mountpoint == NULL)
    {
        return ERROR(EINVAL);
    }

    path_t path;
    if (vfs_lookup(&path, mountpoint) == ERR)
    {
        return ERR;
    }
    PATH_DEFER(&path);

    RWLOCK_READ_DEFER(&root.lock);
    RWLOCK_WRITE_DEFER(&mountCache.lock);
    LOCK_DEFER(&path.dentry->lock);

    if (!(path.dentry->flags & DENTRY_MOUNTPOINT))
    {
        return ERROR(EINVAL);
    }

    map_key_t key = mount_key(path.mount->id, path.dentry->id);
    mount_t* mount = CONTAINER_OF_SAFE(map_get(&mountCache.map, &key), mount_t, mapEntry);
    if (mount == NULL)
    {
        return ERR;
    }
    mount = mount_ref(mount);
    MOUNT_DEFER(mount);

    if (mount == root.mount)
    {
        return ERROR(EBUSY);
    }

    if (atomic_load(&mount->superblock->ref) > 1)
    {
        return ERROR(EBUSY);
    }

    map_remove(&mountCache.map, &key);
    mount_deref(mount); // Remove reference in cache.

    path.dentry->flags &= ~DENTRY_MOUNTPOINT;

    LOG_INFO("vfs: unmounted %s\n", mountpoint);
    return 0;
}

bool vfs_is_name_valid(const char* name)
{
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
        if (vfs_lookup_parent(&parent, parsed.pathname, lastComponent) == ERR)
        {
            return NULL;
        }
        PATH_DEFER(&parent);

        dentry = vfs_get_dentry(parent.dentry, lastComponent);
        if (dentry == NULL)
        {
            if (parent.dentry->inode == NULL || parent.dentry->inode->ops == NULL ||
                parent.dentry->inode->ops->create == NULL)
            {
                return ERRPTR(ENOSYS);
            }

            inode_t* inode = parent.dentry->inode->ops->create(parent.dentry->inode, lastComponent, INODE_NONE);
            if (inode == NULL)
            {
                return NULL;
            }

            INODE_DEFER(inode);

            dentry = dentry_new(parent.dentry, lastComponent, inode);
            if (dentry == NULL)
            {
                return NULL;
            }

            map_key_t key = dentry_key(parent.dentry->id, lastComponent);
            rwlock_write_acquire(&dentryCache.lock);
            map_insert(&dentryCache.map, &key, &dentry->mapEntry);
            rwlock_write_release(&dentryCache.lock);
        }
        else if (parsed.flags & PATH_EXCLUSIVE)
        {
            dentry_deref(dentry);
            return ERRPTR(EEXIST);
        }
    }
    else // Dont create dentry
    {
        path_t path;
        if (vfs_lookup(&path, pathname) == ERR)
        {
            return NULL;
        }

        dentry = dentry_ref(path.dentry);
        path_put(&path);
    }

    *outFlags = parsed.flags;
    return dentry;
}

file_t* vfs_open(const char* pathname)
{
    if (pathname == NULL)
    {
        return ERRPTR(EINVAL);
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

    if ((flags & PATH_TRUNCATE) && dentry->inode->type == INODE_FILE)
    {
        if (dentry->inode->ops == NULL || dentry->inode->ops->truncate == NULL)
        {
            return ERRPTR(ENOSYS);   
        }

        dentry->inode->ops->truncate(dentry->inode);
    }

    if (file->ops != NULL && file->ops->open != NULL)
    {
        uint64_t result = file->ops->open(dentry->inode, file);
        if (result == ERR)
        {
            file_deref(file);
            return NULL;
        }
    }

    return file;
}

uint64_t vfs_open2(const char* pathname, file_t* files[2])
{
    if (pathname == NULL || files == NULL)
    {
        return ERROR(EINVAL);
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
            return ERROR(ENOSYS);   
        }

        dentry->inode->ops->truncate(dentry->inode);
    }

    if (files[0]->ops != NULL && files[0]->ops->open2 != NULL)
    {
        uint64_t result = files[0]->ops->open2(dentry->inode, files);
        if (result == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file == NULL || buffer == NULL)
    {
        return ERROR(EINVAL);
    }

    if (file->ops == NULL || file->ops->read == NULL)
    {
        return ERROR(ENOSYS);
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
        return ERROR(EINVAL);
    }

    if (file->ops == NULL || file->ops->write == NULL)
    {
        return ERROR(ENOSYS);
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
        return ERROR(EINVAL);
    }

    if (file->ops != NULL && file->ops->seek != NULL)
    {
        return file->ops->seek(file, offset, origin);
    }

    return ERROR(ESPIPE);
}

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    return ERROR(ENOSYS);
}

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    return ERRPTR(ENOSYS);
}

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_readdir(file_t* file, stat_t* infos, uint64_t amount)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_mkdir(const char* pathname, uint64_t flags)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_rmdir(const char* pathname)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_stat(const char* pathname, stat_t* buffer)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_link(const char* oldpath, const char* newpath)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_unlink(const char* pathname)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_rename(const char* oldpath, const char* newpath)
{
    return ERROR(ENOSYS);
}

uint64_t vfs_remove(const char* pathname)
{
    return ERROR(ENOSYS);
}