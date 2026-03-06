#include <kernel/fs/namespace.h>

#include <kernel/cpu/syscall.h>
#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>

typedef struct
{
    binding_id_t parentId;
    dentry_id_t locationId;
} binding_key_t;

static bool binding_map_cmp(map_entry_t* entry, const void* key)
{
    binding_stack_t* stack = CONTAINER_OF(entry, binding_stack_t, mapEntry);
    const binding_key_t* k = key;
    return stack->parentId == k->parentId && stack->locationId == k->locationId;
}

static uint64_t binding_hash(binding_id_t parentId, dentry_id_t locationId)
{
    binding_key_t key;
    key.parentId = parentId;
    key.locationId = locationId;
    return hash_buffer(&key, sizeof(key));
}

static status_t binding_stack_push(binding_stack_t* stack, binding_t* binding)
{
    if (stack->count >= ARRAY_SIZE(stack->bindings))
    {
        return ERR(VFS, SHADOW_LIMIT);
    }

    stack->bindings[stack->count] = REF(binding);
    stack->count++;

    return OK;
}

static void binding_stack_remove(binding_stack_t* stack, binding_t* binding)
{
    for (uint64_t i = 0; i < stack->count; i++)
    {
        if (stack->bindings[i] != binding)
        {
            continue;
        }

        UNREF(stack->bindings[i]);
        stack->bindings[i] = NULL;

        memmove(&stack->bindings[i], &stack->bindings[i + 1], (stack->count - i - 1) * sizeof(binding_t*));
        stack->count--;
        break;
    }
}

static void binding_stack_init(namespace_t* ns, binding_stack_t* stack, binding_id_t parentId, dentry_id_t locationId)
{
    list_entry_init(&stack->entry);
    map_entry_init(&stack->mapEntry);
    stack->parentId = parentId;
    stack->locationId = locationId;
    stack->count = 0;

    uint64_t hash = binding_hash(parentId, locationId);
    map_insert(&ns->bindingMap, &stack->mapEntry, hash);
    list_push_back(&ns->stacks, &stack->entry);
}

static void binding_stack_free(namespace_t* ns, binding_stack_t* stack)
{
    if (stack == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < stack->count; i++)
    {
        UNREF(stack->bindings[i]);
        stack->bindings[i] = NULL;
    }
    stack->count = 0;

    list_remove(&stack->entry);
    uint64_t hash = binding_hash(stack->parentId, stack->locationId);
    map_remove(&ns->bindingMap, &stack->mapEntry, hash);

    free(stack);
}

static binding_stack_t* namespace_get_stack(namespace_t* ns, binding_id_t parentId, dentry_id_t locationId)
{
    binding_key_t key = {parentId, locationId};
    uint64_t hash = binding_hash(parentId, locationId);
    return CONTAINER_OF_SAFE(map_find(&ns->bindingMap, &key, hash), binding_stack_t, mapEntry);
}

static status_t namespace_add(namespace_t* ns, binding_t* binding)
{
    binding_id_t parentId = binding->parent->id;
    dentry_id_t locationId = binding->target->id;

    binding_stack_t* stack = namespace_get_stack(ns, parentId, locationId);
    if (stack == NULL)
    {
        stack = malloc(sizeof(binding_stack_t));
        if (stack == NULL)
        {
            return ERR(VFS, NOMEM);
        }

        binding_stack_init(ns, stack, parentId, locationId);
    }

    status_t status = binding_stack_push(stack, binding);
    if (IS_ERR(status))
    {
        return status;
    }

    if (binding->mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            status = namespace_add(child, binding);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }

    return OK;
}

static void namespace_remove(namespace_t* ns, binding_t* binding, mode_t mode)
{
    if (binding->mode & MODE_LOCKED)
    {
        return;
    }

    binding_id_t parentId = binding->parent->id;
    dentry_id_t locationId = binding->target->id;

    binding_stack_t* stack = namespace_get_stack(ns, parentId, locationId);
    if (stack != NULL)
    {
        binding_stack_remove(stack, binding);

        if (stack->count == 0)
        {
            binding_stack_free(ns, stack);
        }
    }

    if (mode & MODE_PROPAGATE)
    {
        namespace_t* child;
        LIST_FOR_EACH(child, &ns->children, entry)
        {
            RWLOCK_WRITE_SCOPE(&child->lock);

            namespace_remove(child, binding, mode);
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
        binding_stack_t* stack = CONTAINER_OF(list_first(&ns->stacks), binding_stack_t, entry);
        binding_stack_free(ns, stack);
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
    MAP_DEFINE_INIT(ns->bindingMap, binding_map_cmp);

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

    binding_stack_t* stack;
    LIST_FOR_EACH(stack, &src->stacks, entry)
    {
        for (uint64_t i = 0; i < stack->count; i++)
        {
            if (stack->bindings[i]->mode & MODE_PRIVATE)
            {
                continue;
            }

            status_t status = namespace_add(dest, stack->bindings[i]);
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

bool namespace_rcu_traverse(namespace_t* ns, binding_t** binding, dentry_t** dentry)
{
    if (ns == NULL || binding == NULL || dentry == NULL || *binding == NULL || *dentry == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&ns->lock);

    bool traversed = false;
    for (uint64_t i = 0; i < NAMESPACE_MAX_TRAVERSE; i++)
    {
        if (atomic_load(&(*dentry)->bindings) == 0)
        {
            return traversed;
        }

        binding_stack_t* stack = namespace_get_stack(ns, (*binding)->id, (*dentry)->id);
        if (stack == NULL)
        {
            return traversed;
        }

        assert(stack->count > 0);
        binding_t* bind = stack->bindings[stack->count - 1];

        *binding = bind;
        *dentry = bind->source;
        traversed = true;
    }

    return traversed;
}

status_t namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode, binding_t** out)
{
    if (ns == NULL || !PATH_IS_VALID(target) || !PATH_IS_VALID(source))
    {
        return ERR(VFS, INVAL);
    }

    status_t status = mode_check(&mode, source->binding->mode);
    if (IS_ERR(status))
    {
        return status;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);

    if (!DENTRY_IS_POSITIVE(source->dentry) || (target != NULL && !DENTRY_IS_POSITIVE(target->dentry)))
    {
        return ERR(VFS, NOENT);
    }

    binding_t* binding = binding_new(source->dentry, target->dentry, target->binding, mode);
    if (binding == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    status = namespace_add(ns, binding);
    if (IS_ERR(status))
    {
        UNREF(binding);
        return status;
    }

    if (out != NULL)
    {
        *out = binding;
    }
    else
    {
        UNREF(binding);
    }

    return OK;
}

void namespace_unbind(namespace_t* ns, binding_t* binding, mode_t mode)
{
    if (ns == NULL || binding == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&ns->lock);
    namespace_remove(ns, binding, mode);
}

SYSCALL_DEFINE(SYS_FS_BIND, fd_t target, fd_t source)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    file_t* targetFile = file_table_get(&process->files, target);
    if (targetFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(targetFile);

    file_t* sourceFile = file_table_get(&process->files, source);
    if (sourceFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(sourceFile);

    return namespace_bind(ns, &targetFile->path, &sourceFile->path, sourceFile->mode, NULL);
}

SYSCALL_DEFINE(SYS_FS_UNBIND, fd_t target)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR(VFS, DYING);
    }
    UNREF_DEFER(ns);

    file_t* targetFile = file_table_get(&process->files, target);
    if (targetFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(targetFile);

    namespace_unbind(ns, targetFile->path.binding, targetFile->mode);
    return OK;
}