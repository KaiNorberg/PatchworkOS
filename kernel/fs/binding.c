#include <kernel/fs/binding.h>

#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <stdlib.h>
#include <sys/list.h>

static void binding_free(binding_t* binding)
{
    if (binding == NULL)
    {
        return;
    }

    if (binding->target != NULL)
    {
        atomic_fetch_sub_explicit(&binding->target->bindings, 1, memory_order_relaxed);
        UNREF(binding->target);
    }

    if (binding->source != NULL)
    {
        UNREF(binding->source);
    }

    if (binding->parent != NULL)
    {
        UNREF(binding->parent);
    }

    rcu_call(&binding->rcu, rcu_call_free, binding);
}

binding_t* binding_new(dentry_t* source, dentry_t* target, binding_t* parent, mode_t mode)
{
    if (source == NULL || (target != NULL && parent == NULL))
    {
        return NULL;
    }

    binding_t* binding = malloc(sizeof(binding_t));
    if (binding == NULL)
    {
        return NULL;
    }

    ref_init(&binding->ref, binding_free);
    binding->id = vfs_id_get();
    binding->source = REF(source);
    if (target != NULL)
    {
        binding->target = REF(target);
        atomic_fetch_add_explicit(&target->bindings, 1, memory_order_relaxed);
    }
    else
    {
        binding->target = NULL;
    }
    binding->parent = parent != NULL ? REF(parent) : NULL;
    binding->mode = mode;

    return binding;
}
