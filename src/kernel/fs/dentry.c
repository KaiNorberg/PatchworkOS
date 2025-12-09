#include <kernel/fs/dentry.h>

#include <kernel/sync/seqlock.h>
#include <stdio.h>

#include <kernel/fs/inode.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>

#include <stdlib.h>

static map_t dentryCache = MAP_CREATE();
static rwlock_t dentryCacheLock = RWLOCK_CREATE();

static map_key_t dentry_cache_key(dentry_id_t parentId, const char* name)
{
    struct
    {
        dentry_id_t parentId;
        char name[MAX_NAME];
    } buffer;
    buffer.parentId = parentId;
    memset(buffer.name, 0, sizeof(buffer.name));
    strncpy_s(buffer.name, sizeof(buffer.name), name, MAX_NAME - 1);

    return map_key_buffer(&buffer, sizeof(buffer));
}

static uint64_t dentry_cache_add(dentry_t* dentry)
{
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);

    RWLOCK_WRITE_SCOPE(&dentryCacheLock);
    if (map_insert(&dentryCache, &key, &dentry->mapEntry) == ERR)
    {
        return ERR;
    }

    return 0;
}

static void dentry_cache_remove(dentry_t* dentry)
{
    RWLOCK_WRITE_SCOPE(&dentryCacheLock);
    map_remove(&dentryCache, &dentry->mapEntry);
}

static dentry_t* dentry_cache_get(map_key_t* key)
{
    RWLOCK_READ_SCOPE(&dentryCacheLock);

    dentry_t* dentry = CONTAINER_OF_SAFE(map_get(&dentryCache, key), dentry_t, mapEntry);
    if (dentry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    if (atomic_load(&dentry->ref.count) == 0) // Is currently being removed
    {
        errno = ENOENT;
        return NULL;
    }

    return REF(dentry);
}

static void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    assert(dentry->parent != NULL);

    dentry_cache_remove(dentry);

    if (dentry->ops != NULL && dentry->ops->cleanup != NULL)
    {
        dentry->ops->cleanup(dentry);
    }

    if (dentry->inode != NULL)
    {
        atomic_fetch_sub_explicit(&dentry->inode->dentryCount, 1, memory_order_relaxed);
        DEREF(dentry->inode);
        dentry->inode = NULL;
    }

    if (!DENTRY_IS_ROOT(dentry))
    {
        inode_t* parentInode = dentry_inode_get(dentry->parent);
        if (parentInode == NULL)
        {
            panic(NULL, "dentry parent inode is NULL during dentry free\n");
        }
        DEREF_DEFER(parentInode);

        mutex_acquire(&parentInode->mutex);
        list_remove(&dentry->parent->children, &dentry->siblingEntry);
        mutex_release(&parentInode->mutex);

        DEREF(dentry->parent);
        dentry->parent = NULL;
    }

    DEREF(dentry->superblock);
    dentry->superblock = NULL;

    free(dentry);
}

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name)
{
    if (name == NULL || superblock == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    size_t nameLen = strnlen_s(name, MAX_NAME);
    if (nameLen >= MAX_NAME || nameLen == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    assert(parent == NULL || superblock == parent->superblock);

    dentry_t* dentry = malloc(sizeof(dentry_t));
    if (dentry == NULL)
    {
        return NULL;
    }

    ref_init(&dentry->ref, dentry_free);
    map_entry_init(&dentry->mapEntry);
    dentry->id = vfs_get_new_id();
    strncpy(dentry->name, name, MAX_NAME - 1);
    dentry->name[MAX_NAME - 1] = '\0';
    lock_init(&dentry->inodeLock);
    dentry->inode = NULL;
    dentry->parent = parent != NULL
        ? REF(parent)
        : dentry; // We set its parent now but its only added to its list when it is made positive.
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = REF(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    atomic_init(&dentry->mountCount, 0);
    list_entry_init(&dentry->otherEntry);

    if (dentry_cache_add(dentry) == ERR)
    {
        DEREF(dentry);
        return NULL;
    }

    return dentry;
}

dentry_t* dentry_get(const dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->id, name);
    return dentry_cache_get(&key);
}

dentry_t* dentry_lookup(const path_t* parent, const char* name)
{
    if (parent == NULL || parent->dentry == NULL || parent->mount == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->dentry->id, name);
    dentry_t* dentry = dentry_cache_get(&key);
    if (dentry != NULL)
    {
        return dentry;
    }

    inode_t* dir = dentry_inode_get(parent->dentry);
    if (dir == NULL)
    {
        errno = ENOENT;
        return NULL;
    }
    DEREF_DEFER(dir);

    dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        // This logic is a bit complex, but im pretty confident its correct.
        if (errno == EEXIST) // Dentry was created after we called dentry_cache_get but before dentry_new
        {
            // If this fails then the dentry was deleted in between dentry_new and here, which should be fine?
            return dentry_cache_get(&key);
        }
        return NULL;
    }
    DEREF_DEFER(dentry);

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    if (dir->ops == NULL || dir->ops->lookup == NULL)
    {
        return REF(dentry); // Leave it negative
    }

    if (dir->ops->lookup(dir, dentry) == ERR)
    {
        return NULL;
    }

    return REF(dentry);
}

void dentry_make_positive(dentry_t* dentry, inode_t* inode)
{
    if (dentry == NULL || inode == NULL)
    {
        return;
    }

    lock_acquire(&dentry->inodeLock);

    assert(dentry->inode == NULL);

    atomic_fetch_add_explicit(&inode->dentryCount, 1, memory_order_relaxed);
    dentry->inode = REF(inode);
    list_push_back(&dentry->parent->children, &dentry->siblingEntry);

    lock_release(&dentry->inodeLock);
}

bool dentry_is_positive(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return false;
    }

    lock_acquire(&dentry->inodeLock);
    bool isPositive = dentry->inode != NULL;
    lock_release(&dentry->inodeLock);

    return isPositive;
}

bool dentry_is_file(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return false;
    }

    lock_acquire(&dentry->inodeLock);
    bool isFile = dentry->inode != NULL && dentry->inode->type == INODE_FILE;
    lock_release(&dentry->inodeLock);
    return isFile;
}

bool dentry_is_dir(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return false;
    }

    lock_acquire(&dentry->inodeLock);
    bool isDir = dentry->inode != NULL && dentry->inode->type == INODE_DIR;
    lock_release(&dentry->inodeLock);
    return isDir;
}

inode_t* dentry_inode_get(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return NULL;
    }

    lock_acquire(&dentry->inodeLock);
    inode_t* inode = dentry->inode != NULL ? REF(dentry->inode) : NULL;
    lock_release(&dentry->inodeLock);
    return inode;
}

typedef struct
{
    uint64_t index;
    dirent_t* buffer;
    uint64_t count;
    uint64_t* offset;
    mode_t mode;
    char basePath[MAX_PATH];
} getdents_ctx_t;

static void getdents_write(getdents_ctx_t* ctx, inode_number_t number, inode_type_t type, const char* name)
{
    uint64_t start = *ctx->offset / sizeof(dirent_t);
    uint64_t amount = ctx->count / sizeof(dirent_t);

    if (ctx->index >= start && ctx->index < start + amount)
    {
        dirent_t* dirent = &ctx->buffer[ctx->index - start];
        dirent->number = number;
        dirent->type = type;
        strncpy(dirent->path, name, MAX_PATH - 1);
        dirent->path[MAX_PATH - 1] = '\0';
    }

    ctx->index++;
}

static void getdents_recursive_traversal(getdents_ctx_t* ctx, dentry_t* dentry)
{
    inode_t* inode = dentry_inode_get(dentry);
    if (inode == NULL)
    {
        errno = ENOENT;
        return;
    }
    DEREF_DEFER(inode);

    getdents_write(ctx, inode->number, inode->type, ctx->basePath);

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        inode_t* childInode = dentry_inode_get(child);
        if (childInode == NULL)
        {
            continue;
        }
        DEREF_DEFER(childInode);

        if (ctx->mode & MODE_RECURSIVE && childInode->type == INODE_DIR)
        {
            char originalBasePath[MAX_PATH];
            strncpy(originalBasePath, ctx->basePath, MAX_PATH - 1);
            originalBasePath[MAX_PATH - 1] = '\0';

            snprintf(ctx->basePath, MAX_PATH, "%s/%s", originalBasePath, child->name);
            getdents_recursive_traversal(ctx, child);

            strncpy(ctx->basePath, originalBasePath, MAX_PATH - 1);
            ctx->basePath[MAX_PATH - 1] = '\0';
        }
        else
        {
            char fullPath[MAX_PATH];
            snprintf(fullPath, MAX_PATH, "%s/%s", ctx->basePath, child->name);
            getdents_write(ctx, childInode->number, childInode->type, fullPath);
        }
    }
}

uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset, mode_t mode)
{
    getdents_ctx_t ctx = {
        .index = 0,
        .buffer = buffer,
        .count = count,
        .offset = offset,
        .mode = mode,
        .basePath = "",
    };

    inode_t* inode = dentry_inode_get(dentry);
    if (inode == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DEREF_DEFER(inode);

    inode_t* parentInode = dentry_inode_get(dentry->parent);
    if (parentInode == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    DEREF_DEFER(parentInode);

    getdents_write(&ctx, inode->number, inode->type, ".");
    getdents_write(&ctx, parentInode->number, parentInode->type, "..");

    if (mode & MODE_RECURSIVE)
    {
        dentry_t* child;
        LIST_FOR_EACH(child, &dentry->children, siblingEntry)
        {
            inode_t* childInode = dentry_inode_get(child);
            if (childInode == NULL)
            {
                continue;
            }
            DEREF_DEFER(childInode);

            if (childInode->type == INODE_DIR)
            {
                snprintf(ctx.basePath, MAX_PATH, "%s", child->name);
                getdents_recursive_traversal(&ctx, child);
                ctx.basePath[0] = '\0';
            }
            else
            {
                getdents_write(&ctx, childInode->number, childInode->type, child->name);
            }
        }
    }
    else
    {
        dentry_t* child;
        LIST_FOR_EACH(child, &dentry->children, siblingEntry)
        {
            inode_t* childInode = dentry_inode_get(child);
            if (childInode == NULL)
            {
                continue;
            }
            DEREF_DEFER(childInode);

            getdents_write(&ctx, childInode->number, childInode->type, child->name);
        }
    }

    uint64_t start = *offset / sizeof(dirent_t);
    if (start >= ctx.index)
    {
        return 0;
    }

    uint64_t entriesWritten = MIN(ctx.index - start, count / sizeof(dirent_t));
    uint64_t bytesWritten = entriesWritten * sizeof(dirent_t);
    *offset += bytesWritten;
    return bytesWritten;
}
