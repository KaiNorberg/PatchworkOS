#include <kernel/fs/namespace.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <stdlib.h>
#include <sys/map.h>
#include <sys/fs.h>
#include <sys/list.h>

typedef struct
{
    mount_id_t parentId;
    dentry_id_t mountpointId;
} mount_key_t;

static bool mount_map_cmp(map_entry_t* entry, const void* key)
{
    mount_stack_t* stack = CONTAINER_OF(entry, mount_stack_t, mapEntry);
    const mount_key_t* k = key;
    return stack->parentId == k->parentId && stack->mountpointId == k->mountpointId;
}

static uint64_t mount_hash(mount_id_t parentId, dentry_id_t mountpointId)
{
    mount_key_t key;
    key.parentId = parentId;
    key.mountpointId = mountpointId;
    return hash_buffer(&key, sizeof(key));
}

static status_t mount_stack_push(mount_stack_t* stack, mount_t* mount)
{
    if (stack->count >= ARRAY_SIZE(stack->mounts))
    {
        return ERR(VFS, SHADOW_LIMIT);
    }

    stack->mounts[stack->count] = REF(mount);
    stack->count++;

    return OK;
}

static void mount_stack_remove(mount_stack_t* stack, mount_t* mount)
{
    for (uint64_t i = 0; i < stack->count; i++)
    {
        if (stack->mounts[i] != mount)
        {
            continue;
        }

        UNREF(stack->mounts[i]);
        stack->mounts[i] = NULL;

        memmove(&stack->mounts[i], &stack->mounts[i + 1], (stack->count - i - 1) * sizeof(mount_t*));
        stack->count--;
        break;
    }
}

static void mount_stack_init(namespace_t* ns, mount_stack_t* stack, mount_id_t parentId, dentry_id_t mountpointId)
{
    list_entry_init(&stack->entry);
    map_entry_init(&stack->mapEntry);
    stack->parentId = parentId;
    stack->mountpointId = mountpointId;
    stack->count = 0;

    uint64_t hash = mount_hash(parentId, mountpointId);
    map_insert(&ns->mountMap, &stack->mapEntry, hash);
    list_push_back(&ns->stacks, &stack->entry);
}

static void mount_stack_free(namespace_t* ns, mount_stack_t* stack)
{
    if (stack == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < stack->count; i++)
    {
        UNREF(stack->mounts[i]);
        stack->mounts[i] = NULL;
    }
    stack->count = 0;

    list_remove(&stack->entry);
    uint64_t hash = mount_hash(stack->parentId, stack->mountpointId);
    map_remove(&ns->mountMap, &stack->mapEntry, hash);

    if (&ns->root == stack)
    {
        return;
    }

    free(stack);
}

static mount_stack_t* namespace_get_stack(namespace_t* ns, mount_id_t parentId, dentry_id_t mountpointId)
{
    mount_key_t key = {parentId, mountpointId};
    uint64_t hash = mount_hash(parentId, mountpointId);
    return CONTAINER_OF_SAFE(map_find(&ns->mountMap, &key, hash), mount_stack_t, mapEntry);
}

static status_t namespace_add(namespace_t* ns, mount_t* mount)
{
    if (MOUNT_IS_ROOT(mount))
    {
        status_t status = mount_stack_push(&ns->root, mount);
        if (IS_ERR(status))
        {
            return status;
        }
        goto propagate;
    }

    mount_id_t parentId = mount->parent->id;
    dentry_id_t mountpointId = mount->target->id;

    mount_stack_t* stack = namespace_get_stack(ns, parentId, mountpointId);
    if (stack == NULL)
    {
        stack = malloc(sizeof(mount_stack_t));
        if (stack == NULL)
        {
            return ERR(VFS, NOMEM);
        }

        mount_stack_init(ns, stack, parentId, mountpointId);
    }

    status_t status = mount_stack_push(stack, mount);
    if (IS_ERR(status))
    {
        return status;
    }

propagate:
    if (mount->mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            status = namespace_add(child, mount);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }

    return OK;
}

static void namespace_remove(namespace_t* ns, mount_t* mount, mode_t mode)
{
    if (mount->mode & MODE_LOCKED)
    {
        return;
    }

    if (MOUNT_IS_ROOT(mount))
    {
        mount_stack_remove(&ns->root, mount);
        goto propagate;
    }

    mount_id_t parentId = mount->parent->id;
    dentry_id_t mountpointId = mount->target->id;

    mount_stack_t* stack = namespace_get_stack(ns, parentId, mountpointId);
    if (stack != NULL)
    {
        mount_stack_remove(stack, mount);

        if (stack->count == 0)
        {
            mount_stack_free(ns, stack);
        }
    }

propagate:
    if (mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            namespace_remove(child, mount, mode);
        }
    }
}

static void namespace_free(namespace_t* ns)
{
    if (ns == NULL)
    {
        return;
    }

    if (ns->parent != NULL)
    {
        rwlock_write_acquire(&ns->parent->lock);
        list_remove(&ns->entry);
        rwlock_write_release(&ns->parent->lock);
        UNREF(ns->parent);
        ns->parent = NULL;
    }

    rwlock_write_acquire(&ns->lock);

    while (!list_is_empty(&ns->stacks))
    {
        mount_stack_t* stack = CONTAINER_OF(list_first(&ns->stacks), mount_stack_t, entry);
        mount_stack_free(ns, stack);
    }

    rwlock_write_release(&ns->lock);

    free(ns);
}

namespace_t* namespace_new(namespace_t* parent)
{
    namespace_t* ns = malloc(sizeof(namespace_t));
    if (ns == NULL)
    {
        return NULL;
    }
    ref_init(&ns->ref, namespace_free);
    list_entry_init(&ns->entry);
    list_init(&ns->children);
    ns->parent = NULL;
    list_init(&ns->stacks);
    MAP_DEFINE_INIT(ns->mountMap, mount_map_cmp);

    mount_stack_init(ns, &ns->root, UINT64_MAX, UINT64_MAX);

    rwlock_init(&ns->lock);

    if (parent != NULL)
    {
        RWLOCK_WRITE_SCOPE(&parent->lock);
        ns->parent = REF(parent);
        list_push_back(&parent->children, &ns->entry);
    }

    return ns;
}

status_t namespace_copy(namespace_t* dest, namespace_t* src)
{
    if (dest == NULL || src == NULL)
    {
        return ERR(VFS, INVAL);
    }

    RWLOCK_WRITE_SCOPE(&dest->lock);
    RWLOCK_WRITE_SCOPE(&src->lock);

    mount_stack_t* stack;
    LIST_FOR_EACH(stack, &src->stacks, entry)
    {
        for (uint64_t i = 0; i < stack->count; i++)
        {
            if (stack->mounts[i]->mode & MODE_PRIVATE)
            {
                continue;
            }

            status_t status = namespace_add(dest, stack->mounts[i]);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }

    return OK;
}

static bool namespace_is_descendant(namespace_t* ancestor, namespace_t* descendant)
{
    // To prevent deadlocks we cant do a search starting from the child, we must always acquire looks from the top down.
    // So this is a bit inefficient.

    if (ancestor == descendant)
    {
        return true;
    }

    namespace_t* child;
    LIST_FOR_EACH(child, &ancestor->children, entry)
    {
        RWLOCK_READ_SCOPE(&child->lock);
        if (namespace_is_descendant(child, descendant))
        {
            return true;
        }
    }

    return false;
}

bool namespace_accessible(namespace_t* ns, namespace_t* other)
{
    if (ns == NULL || other == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&ns->lock);
    return namespace_is_descendant(ns, other);
}

bool namespace_rcu_traverse(namespace_t* ns, mount_t** mount, dentry_t** dentry)
{
    if (ns == NULL || mount == NULL || dentry == NULL || *mount == NULL || *dentry == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    bool traversed = false;
    for (uint64_t i = 0; i < NAMESPACE_MAX_TRAVERSE; i++)
    {
        if (atomic_load(&(*dentry)->mountCount) == 0)
        {
            return traversed;
        }

        mount_stack_t* stack = namespace_get_stack(ns, (*mount)->id, (*dentry)->id);
        if (stack == NULL)
        {
            return traversed;
        }

        assert(stack->count > 0);
        mount_t* mnt = stack->mounts[stack->count - 1];

        *mount = mnt;
        *dentry = mnt->source;
        traversed = true;
    }

    return traversed;
}

status_t namespace_mount(namespace_t* ns, path_t* target, filesystem_t* fs, const char* options, mode_t mode,
    void* data, mount_t** out)
{
    if (ns == NULL || fs == NULL)
    {
        return ERR(VFS, INVAL);
    }

    dentry_t* root;
    status_t status = fs->mount(fs, &root, options, data);
    if (root == NULL)
    {
        return status;
    }
    UNREF_DEFER(root);

    if (root->superblock->root != root)
    {
        return ERR(VFS, IMPL);
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    if (!DENTRY_IS_POSITIVE(root) || (target != NULL && !DENTRY_IS_POSITIVE(target->dentry)))
    {
        return ERR(VFS, NOENT);
    }

    mount_t* mount = mount_new(root->superblock, root, target != NULL ? target->dentry : NULL,
        target != NULL ? target->mount : NULL, mode);
    if (mount == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    status = namespace_add(ns, mount);
    if (IS_ERR(status))
    {
        UNREF(mount);
        return status;
    }

    if (out != NULL)
    {
        *out = mount;
    }
    else
    {
        UNREF(mount);
    }

    return OK;
}

status_t namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode, mount_t** out)
{
    if (ns == NULL || !PATH_IS_VALID(source))
    {
        return ERR(VFS, INVAL);
    }

    status_t status = mode_check(&mode, source->mount->mode);
    if (IS_ERR(status))
    {
        return status;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    if (!DENTRY_IS_POSITIVE(source->dentry) || (target != NULL && !DENTRY_IS_POSITIVE(target->dentry)))
    {
        return ERR(VFS, NOENT);
    }

    mount_t* mount = mount_new(source->dentry->superblock, source->dentry, target != NULL ? target->dentry : NULL,
        target != NULL ? target->mount : NULL, mode);
    if (mount == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    status = namespace_add(ns, mount);
    if (IS_ERR(status))
    {
        UNREF(mount);
        return status;
    }

    if (out != NULL)
    {
        *out = mount;
    }
    else
    {
        UNREF(mount);
    }

    return OK;
}

void namespace_unmount(namespace_t* ns, mount_t* mount, mode_t mode)
{
    if (ns == NULL || mount == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);
    namespace_remove(ns, mount, mode);
}

void namespace_get_root(namespace_t* ns, path_t* out)
{
    if (ns == NULL || out == NULL)
    {
        path_set(out, NULL, NULL);
        return;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    if (ns->root.count == 0)
    {
        path_set(out, NULL, NULL);
        return;
    }

    mount_t* mnt = ns->root.mounts[ns->root.count - 1];
    path_set(out, mnt, mnt->source);
}

void namespace_rcu_get_root(namespace_t* ns, mount_t** mount, dentry_t** dentry)
{
    if (ns == NULL || mount == NULL || dentry == NULL)
    {
        if (mount != NULL)
        {
            *mount = NULL;
        }
        if (dentry != NULL)
        {
            *dentry = NULL;
        }
        return;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    if (ns->root.count == 0)
    {
        *mount = NULL;
        *dentry = NULL;
        return;
    }

    mount_t* mnt = ns->root.mounts[ns->root.count - 1];
    *mount = mnt;
    *dentry = mnt->source;
}

SYSCALL_DEFINE(SYS_MOUNT, const char* mountpoint, const char* fs, const char* options)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t mountname;
    status_t status = thread_copy_from_user_pathname(thread, &mountname, mountpoint);
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

    char fsCopy[MAX_PATH];
    status = thread_copy_from_user_string(thread, fsCopy, fs, MAX_PATH);
    if (IS_ERR(status))
    {
        return status;
    }

    char optionsCopy[MAX_PATH];
    if (options != NULL)
    {
        status = thread_copy_from_user_string(thread, optionsCopy, options, MAX_PATH);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    filesystem_t* filesystem = filesystem_get_by_path(fsCopy, process);
    if (filesystem == NULL)
    {
        return ERR(VFS, NOFS);
    }

    return namespace_mount(ns, &mountpath, filesystem, options != NULL ? optionsCopy : NULL, mountname.mode, NULL, NULL);
}

SYSCALL_DEFINE(SYS_UNMOUNT, const char* mountpoint)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t mountname;
    status_t status = thread_copy_from_user_pathname(thread, &mountname, mountpoint);
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

    namespace_unmount(ns, mountpath.mount, mountname.mode);
    return OK;
}

SYSCALL_DEFINE(SYS_BIND, const char* mountpoint, fd_t source)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    pathname_t mountname;
    status_t status = thread_copy_from_user_pathname(thread, &mountname, mountpoint);
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

    file_t* sourceFile = file_table_get(&process->files, source);
    if (sourceFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(sourceFile);

    return namespace_bind(ns, &mountpath, &sourceFile->path, mountname.mode, NULL);
}