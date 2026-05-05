#include <_libc/MAX_PATH.h>
#include <ctype.h>
#include <kernel/fs/binding_table.h>
#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sync/mutex.h>

#include <kernel/sync/rcu.h>
#include <libc/fs.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline bool path_is_char_valid(char ch)
{
    static const bool forbidden[UINT8_MAX + 1] = {
        [0 ... 31] = true,
        ['<'] = true,
        ['>'] = true,
        [':'] = true,
        ['\"'] = true,
        ['/'] = true,
        ['\\'] = true,
        ['|'] = true,
        ['?'] = true,
        ['*'] = true,
    };
    return !forbidden[(uint8_t)ch];
}

static status_t path_walk_loop(irp_t* irp, path_state_t* state);

static void path_state_free(path_state_t* state)
{
    if (state->root != NULL)
    {
        UNREF(state->root);
    }
    if (state->lookup != NULL)
    {
        UNREF(state->lookup);
    }
    free(state);
}

static void path_state_free_acquired(path_state_t* state)
{
    UNREF(state->dentry);
    UNREF(state->binding);
    path_state_free(state);
}

static inline status_t path_state_acquire(path_state_t* state)
{
    if (REF_TRY(state->dentry) == NULL)
    {
        rcu_read_unlock();
        return ERR(VFS, NOENT);
    }
    if (REF_TRY(state->binding) == NULL)
    {
        UNREF(state->dentry);
        rcu_read_unlock();
        return ERR(VFS, NOENT);
    }

    rcu_read_unlock();
    return OK;
}

static inline void path_state_release(path_state_t* state)
{
    rcu_read_lock();

    UNREF(state->dentry);
    UNREF(state->binding);
}

static status_t path_dotdot(path_state_t* state)
{
    dentry_t* dentry = state->dentry;
    binding_t* binding = state->binding;

    while (dentry == binding->source)
    {
        if (binding->parent == NULL)
        {
            return OK;
        }
        dentry = binding->target;
        binding = binding->parent;
    }

    if (state->root != NULL && dentry == state->root->path.dentry && binding == state->root->path.binding)
    {
        return OK;
    }

    dentry_t* parent = dentry->parent;
    if (parent == NULL)
    {
        return OK;
    }

    if (state->root == NULL)
    {
        return OK;
    }

    dentry_t* check = parent;
    binding_t* checkBinding = binding;
    while (true)
    {
        if (check == state->root->path.dentry && checkBinding == state->root->path.binding)
        {
            state->dentry = parent;
            state->binding = binding;
            return OK;
        }

        if (check == checkBinding->source)
        {
            if (checkBinding->parent == NULL)
            {
                return OK;
            }
            check = checkBinding->target;
            checkBinding = checkBinding->parent;
            continue;
        }

        if (check->parent == NULL)
        {
            return OK;
        }

        check = check->parent;
    }
}

static status_t path_symlink_complete(irp_t* irp, void* ctx)
{
    path_state_t* state = ctx;
    size_t linkLen = irp->result;
    char* link = state->linkBuffer;

    if (IS_ERR(irp->status))
    {
        path_state_free_acquired(state);
        return OK;
    }

    if (linkLen == 0)
    {
        path_state_free_acquired(state);
        return ERR(VFS, IO);
    }

    size_t prefixLen = state->token - state->path;
    size_t suffixLen = state->end - state->ptr;
    size_t newLen = prefixLen + linkLen + suffixLen;

    if (newLen >= MAX_PATH)
    {
        path_state_free_acquired(state);
        return ERR(VFS, PATHTOOLONG);
    }

    memmove(state->token + linkLen, state->ptr, suffixLen);
    memcpy(state->token, link, linkLen);
    state->end = state->path + newLen;

    path_state_release(state);

    if (link[0] == '/')
    {
        if (state->root == NULL)
        {
            rcu_read_unlock();
            path_state_free(state);
            return ERR(VFS, INVAL);
        }

        state->ptr = state->path;
        state->dentry = state->root->path.dentry;
        state->binding = state->root->path.binding;
    }
    else
    {
        state->ptr = state->token;
    }

    irp->status = OK;
    return path_walk_loop(irp, state);
}

static status_t path_symlink(irp_t* irp, path_state_t* state, dentry_t* symlink)
{
    if (++state->symlinkDepth > PATH_MAX_SYMLINK)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, LOOP);
    }

    if (REF_TRY(symlink) == NULL)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, NOENT);
    }

    status_t status = path_state_acquire(state);
    if (IS_ERR(status))
    {
        UNREF(symlink);
        path_state_free(state);
        return status;
    }

    sglist_t* list;
    status = irp_get_sglist(irp, &list);
    if (IS_ERR(status))
    {
        UNREF(symlink);
        path_state_free_acquired(state);
        return status;
    }

    status = sglist_add(list, &process_get_kernel()->space, state->linkBuffer, MAX_PATH);
    if (IS_ERR(status))
    {
        UNREF(symlink);
        path_state_free_acquired(state);
        return status;
    }

    irp_prep_read(irp, list, 0);
    irp_set_complete(irp, path_symlink_complete, state);
    status_t call_status = vnode_call(symlink->vnode, irp);
    UNREF(symlink);
    return call_status;
}

static status_t path_create_complete(irp_t* irp, void* ctx)
{
    path_state_t* state = ctx;

    if (IS_ERR(irp->status))
    {
        path_state_free_acquired(state);
        return OK;
    }

    path_state_release(state);

    state->dentry = state->lookup;
    return path_walk_loop(irp, state);
}

static status_t path_lookup_complete(irp_t* irp, void* ctx)
{
    path_state_t* state = ctx;

    if (!IS_ERR(irp->status) && !DENTRY_IS_POSITIVE(state->lookup))
    {
        irp->status = ERR(VFS, NOENT);
    }

    if (IS_ERR(irp->status))
    {
        while (*state->ptr == '/')
        {
            state->ptr++;
        }
        bool isEnd = state->ptr == state->end;

        if (IS_CODE(irp->status, NOENT))
        {
            if (isEnd && (state->mode & PATH_MODE_CREATE) && !(state->mode & PATH_MODE_EXISTING))
            {
                state->mode &= ~PATH_MODE_EXCLUSIVE;
                irp->status = OK;
                irp_prep_create(irp, state->lookup, state->mode, state->payload);
                irp_set_complete(irp, path_create_complete, state);
                return vnode_call(state->dentry->vnode, irp);
            }

            if (!isEnd && (state->mode & PATH_MODE_PARENTS))
            {
                irp->status = OK;
                irp_prep_create(irp, state->lookup, PATH_MODE_DIRECTORY | PATH_MODE_CREATE, NULL);
                irp_set_complete(irp, path_create_complete, state);
                return vnode_call(state->dentry->vnode, irp);
            }
        }

        path_state_free_acquired(state);
        return OK;
    }

    path_state_release(state);

    if (DENTRY_IS_TYPE(state->lookup, FILE_TYPE_SYMLINK) && !(state->mode & PATH_MODE_NOFOLLOW))
    {
        return path_symlink(irp, state, state->lookup);
    }

    state->dentry = state->lookup;
    return path_walk_loop(irp, state);
}

static status_t path_walk_lookup(irp_t* irp, path_state_t* state, const char* name, size_t len)
{
    status_t status = path_state_acquire(state);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    if (!DENTRY_IS_TYPE(state->dentry, FILE_TYPE_DIRECTORY))
    {
        path_state_free_acquired(state);
        return ERR(VFS, NOTDIR);
    }

    dentry_t* newDentry = dentry_get(state->dentry, name, len);
    if (newDentry == NULL)
    {
        newDentry = dentry_new(state->dentry, name, len);
        if (newDentry == NULL)
        {
            newDentry = dentry_get(state->dentry, name, len);
        }
    }
    if (newDentry == NULL)
    {
        path_state_free_acquired(state);
        return ERR(VFS, NOMEM);
    }

    if (state->lookup != NULL)
    {
        UNREF(state->lookup);
    }
    state->lookup = newDentry;

    if (DENTRY_IS_POSITIVE(state->lookup))
    {
        if (DENTRY_IS_TYPE(state->lookup, FILE_TYPE_SYMLINK) && !(state->mode & PATH_MODE_NOFOLLOW))
        {
            path_state_release(state);
            return path_symlink(irp, state, state->lookup);
        }

        path_state_release(state);
        state->dentry = state->lookup;

        return path_walk_loop(irp, state);
    }

    irp_prep_lookup(irp, state->lookup);
    irp_set_complete(irp, path_lookup_complete, state);
    return vnode_call(state->dentry->vnode, irp);
}

static status_t path_done_complete(irp_t* irp, void* ctx)
{
    UNUSED(irp);

    path_state_t* state = ctx;

    status_t status = state->done(irp, state, state->file);
    path_state_free(state);
    return status;
}

static status_t path_done(irp_t* irp, path_state_t* state)
{
    dentry_t* dentry = REF_TRY(state->dentry);
    if (dentry == NULL)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, NOENT);
    }
    UNREF_DEFER(dentry);

    binding_t* binding = REF_TRY(state->binding);
    if (binding == NULL)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, NOENT);
    }
    UNREF_DEFER(binding);

    if (state->mode & PATH_MODE_EXCLUSIVE)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, EXIST);
    }

    rcu_read_unlock();

    file_t* file = file_new(dentry, binding, state->mode);
    if (file == NULL)
    {
        path_state_free(state);
        return ERR(VFS, NOMEM);
    }
    UNREF_DEFER(file);

    state->file = file;
    if (dentry->vnode->cls->handlers[IRP_MJ_OPEN] == NULL)
    {
        status_t status = state->done(irp, state, state->file);
        path_state_free(state);
        return status;
    }

    irp_prep_open(irp, state->payload);
    irp_set_complete(irp, path_done_complete, state);
    return file_call(file, irp);
}

static status_t path_walk_loop(irp_t* irp, path_state_t* state)
{
    if (state->ptr == state->path && state->ptr < state->end && state->ptr[0] == '/')
    {
        if (state->root == NULL)
        {
            rcu_read_unlock();
            path_state_free(state);
            return ERR(VFS, INVAL);
        }

        state->dentry = state->root->path.dentry;
        state->binding = state->root->path.binding;
        state->ptr++;
    }

    while (true)
    {
        while (state->ptr < state->end && *state->ptr == '/')
        {
            state->ptr++;
        }

        if (state->ptr >= state->end)
        {
            return path_done(irp, state);
        }

        state->token = state->ptr;
        while (state->ptr < state->end && *state->ptr != '/')
        {
            if (!path_is_char_valid(*state->ptr))
            {
                rcu_read_unlock();
                path_state_free(state);
                return ERR(VFS, INVALCHAR);
            }
            state->ptr++;
        }

        state->tokenLength = state->ptr - state->token;
        if (state->tokenLength == 1 && state->token[0] == '.')
        {
            continue;
        }
        if (state->tokenLength == 2 && state->token[0] == '.' && state->token[1] == '.')
        {
            status_t status = path_dotdot(state);
            if (IS_ERR(status))
            {
                rcu_read_unlock();
                path_state_free(state);
                return status;
            }

            continue;
        }

        dentry_t* next = dentry_rcu_get(state->dentry, state->token, state->tokenLength);
        if (next == NULL)
        {
            return path_walk_lookup(irp, state, state->token, state->tokenLength);
        }

        dentry_t* symlinkDentry = next;
        binding_t* symlinkBinding = state->binding;

        if (state->root != NULL && atomic_load(&symlinkDentry->bindings) > 0)
        {
            binding_table_rcu_traverse(&state->root->bindings, &symlinkBinding, &symlinkDentry);
        }

        if (DENTRY_IS_TYPE(symlinkDentry, FILE_TYPE_SYMLINK) && !(state->mode & PATH_MODE_NOFOLLOW))
        {
            return path_symlink(irp, state, symlinkDentry);
        }

        state->dentry = symlinkDentry;
        state->binding = symlinkBinding;
    }
}

static status_t path_verify(path_state_t* state, size_t length)
{
    state->mode = PATH_MODE_NONE;
    state->payload[0] = '\0';

    char* p = state->path;
    char* end = state->path + length;

    uint64_t nameLength = 0;
    while (p < end && *p != ':' && *p != '?')
    {
        if (*p == '/')
        {
            nameLength = 0;
        }
        else
        {
            if (!path_is_char_valid(*p))
            {
                return ERR(VFS, INVALCHAR);
            }
            nameLength++;
            if (nameLength >= MAX_NAME)
            {
                return ERR(VFS, NAMETOOLONG);
            }
        }

        p++;
    }

    state->end = p;

    while (true)
    {
        if (p < end && *p == '?')
        {
            memcpy(state->payload, p + 1, end - p - 1);
            state->payload[end - p - 1] = '\0';
            return OK;
        }

        if (p < end && *p == ':')
        {
            p++;
        }

        if (p >= end)
        {
            return OK;
        }

        char* token = p;
        while (p < end && *p != ':' && *p != '?')
        {
            if (!isalpha(*p))
            {
                return ERR(VFS, INVALCHAR);
            }
            p++;
        }

        size_t tokenLength = p - token;
        path_mode_t mode;
        status_t status = path_string_to_mode(token, tokenLength, &mode);
        if (IS_ERR(status) || mode == PATH_MODE_NONE)
        {
            return ERR(VFS, INVALFLAG);
        }

        state->mode |= mode;
    }

    return OK;
}

status_t path_walk(irp_t* irp, path_state_t* state, size_t length)
{
    status_t status = path_verify(state, length);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    status = mode_check(&state->mode, state->binding->mode);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    rcu_read_lock();

    return path_walk_loop(irp, state);
}

status_t path_to_name(const path_t* path, char* pathname, size_t length)
{
    if (path == NULL || path->dentry == NULL || path->binding == NULL || pathname == NULL)
    {
        return ERR(VFS, INVAL);
    }

    char* ptr = pathname + length - 1;
    *ptr = '\0';

    dentry_t* dentry = path->dentry;
    binding_t* mount = path->binding;

    while (true)
    {
        if (dentry == mount->source)
        {
            if (mount->parent == NULL)
            {
                break;
            }

            dentry = mount->target;
            mount = mount->parent;
            continue;
        }

        if (dentry->parent == NULL)
        {
            return ERR(VFS, NOENT);
        }

        if ((size_t)(ptr - pathname) < dentry->name.length + 1)
        {
            return ERR(VFS, NAMETOOLONG);
        }

        ptr -= dentry->name.length;
        memcpy(ptr, dentry->name.data, dentry->name.length);

        ptr--;
        *ptr = '/';

        dentry = dentry->parent;
    }

    if (*ptr == '\0')
    {
        if (ptr == pathname)
        {
            return ERR(VFS, NAMETOOLONG);
        }
        ptr--;
        *ptr = '/';
    }

    size_t totalLen = (pathname + length - 1) - ptr;
    memmove((void*)pathname, ptr, totalLen + 1);

    return OK;
}

status_t mode_check(path_mode_t* mode, path_mode_t maxPerms)
{
    if (mode == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (((*mode & PATH_MODE_ALL_PERMS) & ~maxPerms) != PATH_MODE_NONE)
    {
        return ERR(VFS, ACCESS);
    }

    if ((*mode & PATH_MODE_ALL_PERMS) == PATH_MODE_NONE)
    {
        *mode |= maxPerms & PATH_MODE_ALL_PERMS;
    }

    return OK;
}
