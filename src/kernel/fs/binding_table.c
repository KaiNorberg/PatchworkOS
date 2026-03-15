#include <kernel/fs/binding_table.h>

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

static void binding_stack_init(binding_table_t* table, binding_stack_t* stack, binding_id_t parentId,
    dentry_id_t locationId)
{
    list_entry_init(&stack->entry);
    map_entry_init(&stack->mapEntry);
    stack->parentId = parentId;
    stack->locationId = locationId;
    stack->count = 0;

    uint64_t hash = binding_hash(parentId, locationId);
    map_insert(&table->bindingMap, &stack->mapEntry, hash);
    list_push_back(&table->stacks, &stack->entry);
}

static void binding_stack_free(binding_table_t* table, binding_stack_t* stack)
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
    map_remove(&table->bindingMap, &stack->mapEntry, hash);

    free(stack);
}

static binding_stack_t* binding_table_get_stack(binding_table_t* table, binding_id_t parentId, dentry_id_t locationId)
{
    binding_key_t key = {parentId, locationId};
    uint64_t hash = binding_hash(parentId, locationId);
    return CONTAINER_OF_SAFE(map_find(&table->bindingMap, &key, hash), binding_stack_t, mapEntry);
}

static status_t binding_table_add(binding_table_t* table, binding_t* binding)
{
    binding_id_t parentId = binding->parent->id;
    dentry_id_t locationId = binding->target->id;

    binding_stack_t* stack = binding_table_get_stack(table, parentId, locationId);
    if (stack == NULL)
    {
        stack = malloc(sizeof(binding_stack_t));
        if (stack == NULL)
        {
            return ERR(VFS, NOMEM);
        }

        binding_stack_init(table, stack, parentId, locationId);
    }

    status_t status = binding_stack_push(stack, binding);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

static void binding_table_remove(binding_table_t* table, binding_t* binding)
{
    if (binding->mode & MODE_LOCKED)
    {
        return;
    }

    binding_id_t parentId = binding->parent->id;
    dentry_id_t locationId = binding->target->id;

    binding_stack_t* stack = binding_table_get_stack(table, parentId, locationId);
    if (stack != NULL)
    {
        binding_stack_remove(stack, binding);

        if (stack->count == 0)
        {
            binding_stack_free(table, stack);
        }
    }
}

void binding_table_deinit(binding_table_t* table)
{
    if (table == NULL)
    {
        return;
    }

    rwlock_write_acquire(&table->lock);

    while (!list_is_empty(&table->stacks))
    {
        binding_stack_t* stack = CONTAINER_OF(list_first(&table->stacks), binding_stack_t, entry);
        binding_stack_free(table, stack);
    }

    rwlock_write_release(&table->lock);
}

void binding_table_init(binding_table_t* table)
{
    list_init(&table->stacks);
    MAP_DEFINE_INIT(table->bindingMap, binding_map_cmp);
    rwlock_init(&table->lock);
}

status_t binding_table_copy(binding_table_t* dest, binding_table_t* src)
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
            status_t status = binding_table_add(dest, stack->bindings[i]);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }

    return OK;
}

bool binding_table_rcu_traverse(binding_table_t* table, binding_t** binding, dentry_t** dentry)
{
    if (table == NULL || binding == NULL || dentry == NULL || *binding == NULL || *dentry == NULL)
    {
        return false;
    }

    RWLOCK_READ_SCOPE(&table->lock);

    bool traversed = false;
    for (uint64_t i = 0; i < BINDING_TABLE_MAX_TRAVERSE; i++)
    {
        if (atomic_load(&(*dentry)->bindings) == 0)
        {
            return traversed;
        }

        binding_stack_t* stack = binding_table_get_stack(table, (*binding)->id, (*dentry)->id);
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

status_t binding_table_bind(binding_table_t* table, path_t* target, path_t* source, mode_t mode, binding_t** out)
{
    if (table == NULL || !PATH_IS_VALID(target) || !PATH_IS_VALID(source))
    {
        return ERR(VFS, INVAL);
    }

    status_t status = mode_check(&mode, source->binding->mode);
    if (IS_ERR(status))
    {
        return status;
    }

    RWLOCK_WRITE_SCOPE(&table->lock);

    if (!DENTRY_IS_POSITIVE(source->dentry) || (target != NULL && !DENTRY_IS_POSITIVE(target->dentry)))
    {
        return ERR(VFS, NOENT);
    }

    binding_t* binding = binding_new(source->dentry, target->dentry, target->binding, mode);
    if (binding == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    status = binding_table_add(table, binding);
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

void binding_table_unbind(binding_table_t* table, binding_t* binding)
{
    if (table == NULL || binding == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&table->lock);
    binding_table_remove(table, binding);
}

SYSCALL_DEFINE(SYS_FD_BIND, fd_t root, fd_t target, fd_t source)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* rootFile = file_table_get(&process->files, root);
    if (rootFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(rootFile);

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

    return binding_table_bind(&rootFile->bindings, &targetFile->path, &sourceFile->path, sourceFile->mode, NULL);
}

SYSCALL_DEFINE(SYS_FD_UNBIND, fd_t root, fd_t target)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* rootFile = file_table_get(&process->files, root);
    if (rootFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(rootFile);

    file_t* targetFile = file_table_get(&process->files, target);
    if (targetFile == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(targetFile);

    binding_table_unbind(&rootFile->bindings, targetFile->path.binding);
    return OK;
}