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

static vfs_map_t flagMap;
static path_flag_entry_t flagEntries[] = {
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_NONBLOCK, .name = "nonblock"},
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_APPEND, .name = "append"},
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_CREATE, .name = "create"},
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_EXCLUSIVE, .name = "exclusive"},
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_TRUNCATE, .name = "trunc"},
    {.entry = MAP_ENTRY_CREATE(), .flag = PATH_DIRECTORY, .name = "dir"},
};

static vfs_root_t root;

static map_key_t inode_key(superblock_id_t superblockId, inode_id_t inodeId)
{
    uint64_t buffer[2] = {(uint64_t)superblockId, (uint64_t)inodeId};

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
    list_init(&map->map);
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

    vfs_map_init(&flagMap);

    root.mount = NULL;
    rwlock_init(&root.lock);

    for (uint64_t i = 0; i < sizeof(flagEntries) / sizeof(flagEntries[0]); i++)
    {
        map_key_t key = map_key_string(flagEntries[i].name);
        assert(map_insert(&flagMap, &key, &flagEntries[i].entry) != ERR);
    }
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

filesystem_t* vfs_get_filesystem(const char* name)
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

inode_t* vfs_get_inode(superblock_t* superblock, inode_id_t id)
{
    return ERROR(ENOSYS);
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

    inode = inode_new(superblock);
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

uint64_t vfs_parse_pathname(parsed_pathname_t* dest, const char* pathname)
{
    const char* flags = NULL;

    const char* p = pathname;
    while (*p != '\0')
    {
        uint64_t len = p - pathname;
        if (len >= MAX_PATH)
        {
            return ERROR(ENAMETOOLONG);
        }

        if (*p == '?')
        {
            if (flags != NULL) // There should only be one ? char in the path.
            {
                return ERROR(EBADFLAG);
            }
            flags = p + 1;
        }
        if (flags == NULL) // If we have not yet encountered a ? char then we are not parsing the flags.
        {
            dest->pathname[len] = *p;
        }

        p++;
    }

    uint64_t len = flags - pathname;
    dest->pathname[len] = '\0';

    dest->flags = PATH_NONE;

    while (true)
    {
        while (*p == '&')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        const char* start = p;
        while (*p != '\0' && *p != '&')
        {
            p++;
        }

        uint64_t len = p - start;
        if (len >= MAX_NAME)
        {
            return ERROR(ENAMETOOLONG);
        }

        map_key_t key = map_key_buffer(start, len);
        path_flag_entry_t* flag = CONTAINER_OF_SAFE(map_get(&flagMap, &key), path_flag_entry_t, entry);
        if (flag == NULL)
        {
            return ERROR(EBADFLAG);
        }
        dest->flags |= flag->flag;
    }
}

static uint64_t vfs_handle_dotdot(path_t* current)
{
    if (current->dentry == current->mount->superblock->root)
    {
        uint64_t iter = 0;

        while (current->dentry == current->mount->superblock->root && iter < VFS_HANDLE_DOTDOT_MAX_ITER)
        {
            if (current->mount->parent == NULL || current->mount->mountpoint == NULL)
            {
                return 0;
            }

            mount_t* newMount = mount_ref(current->mount->parent);
            dentry_t* newDentry = dentry_ref(current->mount->mountpoint);

            mount_deref(current->mount);
            current->mount = newMount;
            dentry_deref(current->dentry);
            current->dentry = newDentry;

            iter++;
        }

        if (iter >= VFS_HANDLE_DOTDOT_MAX_ITER)
        {
            return ERROR(ELOOP);
        }

        if (current->dentry != current->mount->superblock->root)
        {
            dentry_t* parent = current->dentry->parent;
            if (parent == NULL)
            {
                return ERROR(ENOENT);
            }

            dentry_t* new_parent = dentry_ref(parent);
            dentry_deref(current->dentry);
            current->dentry = new_parent;
        }

        return 0;
    }
    else
    {
        assert(current->dentry->parent != NULL); // This can only happen if the filesystem is corrupt.
        dentry_t* parent = dentry_ref(current->dentry->parent);
        dentry_deref(current->dentry);
        current->dentry = parent;

        return 0;
    }
}

static uint64_t vfs_traverse_component(path_t* current, const char* component)
{
    lock_acquire(&current->dentry->lock);
    if (current->dentry->flags & DENTRY_MOUNTPOINT)
    {
        map_key_t mountKey = mount_key(current->mount->id, current->dentry->id);

        rwlock_read_lock(&mountCache.lock);
        mount_t* foundMount = CONTAINER_OF_SAFE(map_get(&mountCache.map, &mountKey), mount_t, mapEntry);
        if (foundMount == NULL)
        {
            rwlock_read_unlock(&mountCache.lock);
            lock_release(&current->dentry->lock);
            return ERROR(ENOENT);
        }

        if (foundMount->superblock == NULL || foundMount->superblock->root == NULL)
        {
            rwlock_read_unlock(&mountCache.lock);
            lock_release(&current->dentry->lock);
            return ERROR(ESTALE);
        }

        mount_t* newMnt = mount_ref(foundMount);
        dentry_t* newRoot = dentry_ref(foundMount->superblock->root);
        rwlock_read_unlock(&mountCache.lock);

        mount_deref(current->mount);
        dentry_deref(current->dentry);
        current->mount = newMnt;
        current->dentry = newRoot;
        return 0;
    }
    lock_release(&current->dentry->lock);

    dentry_t* next = vfs_get_dentry(current->dentry, component);
    if (next == NULL)
    {
        return ERR;
    }

    dentry_deref(current->dentry);
    current->dentry = next;

    return 0;
}

uint64_t vfs_path_walk(path_t* outPath, const char* pathname, const path_t* start)
{
    if (pathname == NULL)
    {
        return ERROR(EINVAL);
    }

    if (pathname[0] == '\0')
    {
        return ERROR(EINVAL);
    }

    path_t current = {0};
    const char* p = pathname;

    if (pathname[0] == '/')
    {
        if (path_get_root(&current) == ERR)
        {
            return ERR;
        }
        p++;
    }
    else
    {
        if (start == NULL)
        {
            return ERROR(EINVAL);
        }
        current.dentry = dentry_ref(start->dentry);
        current.mount = mount_ref(start->mount);
    }

    if (*p == '\0')
    {
        path_copy(outPath, &current);
        path_put(&current);
        return 0;
    }

    char component[MAX_NAME];
    while (*p != '\0')
    {
        while (*p == '/')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        if (*p == '?')
        {
            path_put(&current);
            return ERROR(EBADFLAG);
        }

        const char* componentStart = p;
        while (*p != '\0' && *p != '/')
        {
            if (!VFS_VALID_CHAR(*p))
            {
                path_put(&current);
                return ERROR(EINVAL);
            }
            p++;
        }

        uint64_t len = p - componentStart;
        if (len >= MAX_NAME)
        {
            path_put(&current);
            return ERROR(ENAMETOOLONG);
        }

        memcpy(component, componentStart, len);
        component[len] = '\0';

        if (strcmp(component, ".") == 0)
        {
            continue;
        }

        if (strcmp(component, "..") == 0)
        {
            if (vfs_handle_dotdot(&current) == ERR)
            {
                path_put(&current);
                return ERR;
            }
            continue;
        }

        if (vfs_traverse_component(&current, component) == ERR)
        {
            path_put(&current);
            return ERR;
        }
    }

    path_copy(outPath, &current);
    path_put(&current);
    return 0;
}

uint64_t vfs_path_walk_parent(path_t* outPath, const char* pathname, const path_t* start, char* outLastName)
{
    if (pathname == NULL || outLastName == NULL)
    {
        return ERR;
    }

    memset(outLastName, 0, MAX_NAME);

    const char* lastSlash = strrchr(pathname, '/');
    if (lastSlash == NULL)
    {
        strcpy(outLastName, pathname);
        path_copy(outPath, start);
        return 0;
    }

    strcpy(outLastName, lastSlash + 1);

    if (lastSlash == pathname)
    {
        RWLOCK_READ_DEFER(&root.lock);
        outPath->dentry = dentry_ref(root.mount->superblock->root);
        outPath->mount = mount_ref(root.mount);
        return 0;
    }

    uint64_t parentLen = lastSlash - pathname;
    char parentPath[MAX_PATH];

    memcpy(parentPath, pathname, parentLen);
    parentPath[parentLen] = '\0';

    return vfs_path_walk(outPath, parentPath, start);
}

uint64_t vfs_lookup(path_t* outPath, const char* pathname)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return vfs_path_walk(outPath, pathname, &cwd);
}

uint64_t vfs_lookup_parent(path_t* outPath, const char* pathname, char* outLastName)
{
    path_t cwd;
    vfs_ctx_get_cwd(&sched_process()->vfsCtx, &cwd);
    PATH_DEFER(&cwd);

    return vfs_path_walk_parent(outPath, pathname, &cwd, outLastName);
}

uint64_t vfs_mount(const char* deviceName, const char* mountpoint, const char* fsName, superblock_flags_t flags,
    const void* data)
{
    if (deviceName == NULL || mountpoint == NULL || fsName == NULL)
    {
        return ERROR(EINVAL);
    }

    filesystem_t* fs = vfs_get_filesystem(fsName);
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

    if (root.mount == NULL) // Special handling for mounting inital file system, since this can only happen during boot there is no need for the lock before the if statement.§
    {
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

file_t* vfs_open(const char* pathname)
{
    if (pathname == NULL)
    {
        return ERRPTR(EINVAL);
    }

    parsed_pathname_t parsed;
    if (vfs_parse_pathname(&parsed, pathname) == ERR)
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
            if (parent.dentry->inode == NULL || parent.dentry->inode->ops == NULL || parent.dentry->inode->ops->create == NULL)
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
            return ERROR(EEXIST);
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

    DENTRY_DEFER(dentry);

    file_t* file = file_new(dentry, parsed.flags);
    if (file == NULL)
    {
        return NULL;
    }

    if ((parsed.flags & PATH_TRUNCATE) && dentry->inode->type == INODE_FILE)
    {
        dentry->inode->size = 0;
        dentry->inode->modifyTime = systime_uptime();
        inode_sync(dentry->inode);
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

superblock_t* superblock_new(const char* deviceName, const char* fsName, super_ops_t* ops, dentry_ops_t* dentryOps)
{
    superblock_t* superblock = heap_alloc(sizeof(superblock_t), HEAP_NONE);
    if (superblock == NULL)
    {
        return NULL;
    }

    list_entry_init(&superblock->entry);
    superblock->id = atomic_fetch_add(&newVfsId, 1);
    atomic_init(&superblock->ref, 1);
    superblock->blockSize = PAGE_SIZE;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = SUPER_NONE;
    superblock->private = NULL;
    superblock->root = NULL;
    superblock->ops = ops;
    superblock->ops = dentryOps;
    strncpy(superblock->deviceName, deviceName, MAX_NAME - 1);
    superblock->deviceName[MAX_NAME - 1] = '\0';
    strncpy(superblock->fsName, fsName, MAX_NAME - 1);
    superblock->fsName[MAX_NAME - 1] = '\0';
    // superblock::sysdir is exposed in vfs_mount
    return superblock;
}

void superblock_free(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    if (superblock->root != NULL)
    {
        dentry_deref(superblock->root);
    }

    if (superblock->ops != NULL && superblock->ops->free != NULL)
    {
        superblock->ops->free(superblock);
    }

    heap_free(superblock);
}

superblock_t* superblock_ref(superblock_t* superblock)
{
    if (superblock != NULL)
    {
        atomic_fetch_add(&superblock->ref, 1);
    }
    return superblock;
}

void superblock_deref(superblock_t* superblock)
{
    if (superblock != NULL && atomic_fetch_sub(&superblock->ref, 1) <= 1)
    {
        rwlock_write_acquire(&superblocks.lock);
        list_remove(&superblock->entry);
        rwlock_write_release(&superblocks.lock);
        superblock_free(superblock);
    }
}

inode_t* inode_new(superblock_t* superblock, inode_type_t type, inode_ops_t* ops, file_ops_t* fileOps)
{
    if (superblock == NULL)
    {
        return NULL;
    }

    inode_t* inode;
    if (superblock->ops != NULL && superblock->ops->allocItem != NULL)
    {
        inode = superblock->ops->allocItem(superblock);
    }
    else
    {
        inode = heap_alloc(sizeof(inode_t), HEAP_NONE);
    }

    map_entry_init(&inode->mapEntry);
    inode->id = 0;
    atomic_init(&inode->ref, 1);
    inode->type = type;
    inode->mode = INODE_NONE;
    inode->size = 0;
    inode->blocks = 0;
    inode->blockSize = superblock->blockSize;
    inode->accessTime = systime_uptime();
    inode->modifyTime = inode->accessTime;
    inode->changeTime = inode->accessTime;
    inode->linkCount = 1;
    inode->private = NULL;
    inode->superblock = superblock_ref(superblock);
    inode->ops = ops;
    inode->fileOps = fileOps;

    return inode;
}

void inode_free(inode_t* inode)
{
    if (inode == NULL)
    {
        return;
    }

    if (inode->superblock != NULL)
    {
        if (inode->superblock->ops != NULL && inode->superblock->ops->freeItem != NULL)
        {
            inode->superblock->ops->freeItem(inode->superblock, inode);
        }
        superblock_deref(inode->superblock);
    }

    // If freeItem was not called free manually.
    if (inode->superblock == NULL || inode->superblock->ops == NULL || inode->superblock->ops->freeItem == NULL)
    {
        heap_free(inode);
    }
}

inode_t* inode_ref(inode_t* inode)
{
    if (inode != NULL)
    {
        atomic_fetch_add(&inode->ref, 1);
    }
    return inode;
}

void inode_deref(inode_t* inode)
{
    if (inode != NULL && atomic_fetch_sub(&inode->ref, 1) <= 1)
    {
        rwlock_write_acquire(&inodeCache.lock);
        map_key_t key = inode_key(inode->superblock->id, inode->id);
        map_remove(&inodeCache.map, &key);
        rwlock_write_release(&inodeCache.lock);
        inode_free(inode);
    }
}

uint64_t inode_sync(inode_t* inode)
{
    if (inode == NULL)
    {
        return ERROR(EINVAL);
    }

    if (inode->superblock->ops != NULL && inode->superblock->ops->writeInode != NULL)
    {
        return inode->superblock->ops->writeInode(inode->superblock, inode);
    }

    return 0;
}

dentry_t* dentry_new(dentry_t* parent, const char* name, inode_t* inode)
{
    if (strnlen_s(name, MAX_NAME) >= MAX_NAME)
    {
        return ERROR(EINVAL);
    }

    dentry_t* dentry = heap_alloc(sizeof(dentry_t), HEAP_NONE);
    if (dentry == NULL)
    {
        return NULL;
    }

    map_entry_init(&dentry->mapEntry);
    dentry->id = atomic_fetch_add(&newVfsId, 1);
    atomic_init(&dentry->ref, 1);
    strcpy(dentry->name, name);
    dentry->inode = inode;
    dentry->parent = parent != NULL ? dentry_ref(parent) : dentry; // If a parent is not assigned then the dentry becomes a root entry.
    dentry->superblock = parent != NULL ? superblock_ref(parent->superblock) : NULL; // If this is a root entry then superblock must be set by the caller.
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    dentry->flags = DENTRY_NONE;
    lock_init(&dentry->lock);

    return dentry;
}

void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    if (dentry->inode != NULL)
    {
        inode_deref(dentry->inode);
    }

    if (dentry->parent != NULL && !DETNRY_IS_ROOT(dentry))
    {
        dentry_deref(dentry->parent);
    }

    if (dentry->ops != NULL && dentry->ops->free != NULL)
    {
        dentry->ops->free(dentry);
    }

    heap_free(dentry);
}

dentry_t* dentry_ref(dentry_t* dentry)
{
    if (dentry != NULL)
    {
        atomic_fetch_add(&dentry->ref, 1);
    }
    return dentry;
}

void dentry_deref(dentry_t* dentry)
{
    if (dentry != NULL && atomic_fetch_sub(&dentry->ref, 1) <= 1)
    {
        rwlock_write_acquire(&dentryCache.lock);
        map_key_t key = dentry_key(dentry->parent->id, dentry->name);
        map_remove(&dentryCache.map, &key);
        rwlock_write_release(&dentryCache.lock);

        dentry_free(dentry);
    }
}

file_t* file_new(dentry_t* dentry, path_flags_t flags)
{
    file_t* file = heap_alloc(sizeof(file_t), HEAP_NONE);
    if (file == NULL)
    {
        return NULL;
    }

    atomic_init(&file->ref, 1);
    file->pos = 0;
    file->flags = flags;
    file->dentry = dentry_ref(dentry);
    file->ops = dentry->inode->fileOps;
    file->private = NULL;

    return file;
}

void file_free(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->dentry != NULL)
    {
        dentry_deref(file->dentry);
    }

    heap_free(file);
}

file_t* file_ref(file_t* file)
{
    if (file != NULL)
    {
        atomic_fetch_add(&file->ref, 1);
    }
    return file;
}

void file_deref(file_t* file)
{
    if (file != NULL && atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        if (file->ops != NULL && file->ops->free != NULL)
        {
            file->ops->free(file);
        }
        file_free(file);
    }
}

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint)
{
    mount_t* mount = heap_alloc(sizeof(mount_t), HEAP_NONE);
    if (mount == NULL)
    {
        return NULL;
    }

    map_entry_init(&mount->mapEntry);
    mount->id = atomic_fetch_add(&newVfsId, 1);
    atomic_init(&mount->ref, 1);
    mount->superblock = superblock_ref(superblock);
    mount->mountpoint = mountpoint != NULL ? dentry_ref(mountpoint->dentry) : NULL;
    mount->parent = mountpoint != NULL ? mount_ref(mountpoint->mount) : NULL;

    return mount;
}

void mount_free(mount_t* mount)
{
    if (mount == NULL)
    {
        return;
    }

    if (mount->superblock != NULL)
    {
        superblock_deref(mount->superblock);
    }

    if (mount->mountpoint != NULL)
    {
        dentry_deref(mount->mountpoint);
    }

    if (mount->parent != NULL)
    {
        mount_deref(mount->parent);
    }

    heap_free(mount);
}

mount_t* mount_ref(mount_t* mount)
{
    if (mount != NULL)
    {
        atomic_fetch_add(&mount->ref, 1);
    }
    return mount;
}

void mount_deref(mount_t* mount)
{
    if (mount != NULL && atomic_fetch_sub(&mount->ref, 1) <= 1)
    {
        mount_free(mount);
    }
}

uint64_t path_get_root(path_t* outPath)
{
    rwlock_read_acquire(&root.lock);

    if (root.mount == NULL || root.mount->superblock->root == NULL)
    {
        rwlock_read_cleanup(&root.lock);
        return ERROR(ENOENT);
    }

    outPath->mount = mount_ref(root.mount);
    outPath->dentry = dentry_ref(root.mount->superblock->root);

    rwlock_read_cleanup(&root.lock);
    return 0;
}

void path_copy(path_t* dest, const path_t* src)
{
    dest->dentry = NULL;
    dest->mount = NULL;

    if (src->dentry != NULL)
    {
        dest->dentry = dentry_ref(src->dentry);
    }

    if (src->mount != NULL)
    {
        dest->mount = mount_ref(src->mount);
    }
}

void path_put(path_t* path)
{
    if (path->dentry != NULL)
    {
        dentry_deref(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        mount_deref(path->mount);
        path->mount = NULL;
    }
}