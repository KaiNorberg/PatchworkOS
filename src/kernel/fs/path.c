#include <_libstd/MAX_PATH.h>
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

typedef struct path_flag_short
{
    mode_t mode;
} path_flag_short_t;

static path_flag_short_t shortFlags[UINT8_MAX + 1] = {
    ['r'] = {.mode = MODE_READ},
    ['w'] = {.mode = MODE_WRITE},
    ['x'] = {.mode = MODE_EXECUTE},
    ['a'] = {.mode = MODE_APPEND},
    ['c'] = {.mode = MODE_CREATE},
    ['d'] = {.mode = MODE_DIRECTORY | MODE_CREATE},
    ['s'] = {.mode = MODE_SYMLINK | MODE_CREATE},
    ['h'] = {.mode = MODE_HARDLINK | MODE_CREATE},
    ['e'] = {.mode = MODE_EXCLUSIVE},
    ['E'] = {.mode = MODE_EXISTING},
    ['t'] = {.mode = MODE_TRUNCATE},
    ['l'] = {.mode = MODE_NOFOLLOW},
    ['p'] = {.mode = MODE_PARENTS},
    ['P'] = {.mode = MODE_PRIVATE},
    ['L'] = {.mode = MODE_LOCKED},
};

typedef struct path_flag
{
    mode_t mode;
    const char* name;
} path_flag_t;

static const path_flag_t flags[] = {
    {.mode = MODE_READ, .name = "read"},
    {.mode = MODE_WRITE, .name = "write"},
    {.mode = MODE_EXECUTE, .name = "execute"},
    {.mode = MODE_APPEND, .name = "append"},
    {.mode = MODE_CREATE, .name = "create"},
    {.mode = MODE_DIRECTORY | MODE_CREATE, .name = "directory"},
    {.mode = MODE_SYMLINK | MODE_CREATE, .name = "symlink"},
    {.mode = MODE_HARDLINK | MODE_CREATE, .name = "hardlink"},
    {.mode = MODE_EXCLUSIVE, .name = "exclusive"},
    {.mode = MODE_EXISTING, .name = "existing"},
    {.mode = MODE_TRUNCATE, .name = "truncate"},
    {.mode = MODE_NOFOLLOW, .name = "nofollow"},
    {.mode = MODE_PRIVATE, .name = "private"},
    {.mode = MODE_PARENTS, .name = "parents"},
    {.mode = MODE_LOCKED, .name = "locked"},
};

static mode_t path_flag_to_mode(const char* flag, size_t length)
{
    if (flag == NULL || length == 0)
    {
        return MODE_NONE;
    }

    for (size_t i = 0; i < ARRAY_SIZE(flags); i++)
    {
        size_t len = strnlen_s(flags[i].name, MAX_NAME);
        if (len == length && strncmp(flag, flags[i].name, length) == 0)
        {
            return flags[i].mode;
        }
    }

    mode_t combinedMode = MODE_NONE;
    for (size_t i = 0; i < length; i++)
    {
        if (flag[i] < 0 || (uint8_t)flag[i] >= INT8_MAX)
        {
            return MODE_NONE;
        }
        mode_t mode = shortFlags[(uint8_t)flag[i]].mode;
        if (mode == MODE_NONE)
        {
            return MODE_NONE;
        }
        combinedMode |= mode;
    }

    return combinedMode;
}

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
        return ERR(VFS, NOENT);
    }
    if (REF_TRY(state->binding) == NULL)
    {
        UNREF(state->dentry);
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

    if (link[0] == '/')
    {
        if (state->root == NULL)
        {
            path_state_free_acquired(state);
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

    path_state_release(state);
    return path_walk_loop(irp, state);
}

static status_t path_symlink(irp_t* irp, path_state_t* state, dentry_t* symlink)
{
    if (++state->symlinkDepth > PATH_MAX_SYMLINK)
    {
        path_state_free(state);
        return ERR(VFS, LOOP);
    }

    status_t status = path_state_acquire(state);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    mdl_t* mdl;
    status = irp_get_mdl(irp, &mdl);
    if (IS_ERR(status))
    {
        path_state_release(state);
        path_state_free(state);
        return status;
    }

    status = mdl_add(mdl, &process_get_kernel()->space, state->linkBuffer, MAX_PATH);
    if (IS_ERR(status))
    {
        path_state_release(state);
        path_state_free(state);
        return status;
    }

    irp_prep_read(irp, mdl, 0);
    irp_set_complete(irp, path_symlink_complete, state);
    return vnode_call(symlink->vnode, irp);
}

static status_t path_create_complete(irp_t* irp, void* ctx)
{
    path_state_t* state = ctx;

    if (IS_ERR(irp->status))
    {
        path_state_free(state);
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
            if (isEnd && (state->mode & MODE_CREATE) && !(state->mode & MODE_EXISTING))
            {
                state->mode &= ~MODE_EXCLUSIVE;
                irp->status = OK;
                irp_prep_create(irp, state->lookup, state->mode, state->payload);
                irp_set_complete(irp, path_create_complete, state);
                return vnode_call(state->dentry->vnode, irp);
            }

            if (!isEnd && (state->mode & MODE_PARENTS))
            {
                irp->status = OK;
                irp_prep_create(irp, state->lookup, MODE_DIRECTORY | MODE_CREATE, NULL);
                irp_set_complete(irp, path_create_complete, state);
                return vnode_call(state->dentry->vnode, irp);
            }
        }

        path_state_free(state);
        return OK;
    }

    path_state_release(state);

    state->dentry = state->lookup;
    state->lookup = NULL;

    if (DENTRY_IS_TYPE(state->dentry, FILE_TYPE_SYMLINK) && !(state->mode & MODE_NOFOLLOW))
    {
        return path_symlink(irp, state, state->dentry);
    }

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

    /// @todo Optimize by removing the copy, perhaps add a length string system?
    char buffer[MAX_NAME];
    if (len >= MAX_NAME)
    {
        path_state_free_acquired(state);
        return ERR(VFS, NAMETOOLONG);
    }
    memcpy(buffer, name, len);
    buffer[len] = '\0';

    dentry_t* newDentry = dentry_new(state->dentry, buffer);
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

    binding_t* mount = REF_TRY(state->binding);
    if (mount == NULL)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, NOENT);
    }
    UNREF_DEFER(mount);

    if (state->mode & MODE_EXCLUSIVE)
    {
        rcu_read_unlock();
        path_state_free(state);
        return ERR(VFS, EXIST);
    }

    rcu_read_unlock();

    file_t* file = file_new(dentry, mount, state->mode);
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
                return status;
            }

            continue;
        }

        dentry_t* next = dentry_rcu_get(state->dentry, state->token, state->tokenLength);
        if (next == NULL)
        {
            return path_walk_lookup(irp, state, state->token, state->tokenLength);
        }

        state->dentry = next;

        if (state->root != NULL && atomic_load(&state->dentry->bindings) > 0)
        {
            binding_table_rcu_traverse(&state->root->bindings, &state->binding, &state->dentry);
        }

        if (DENTRY_IS_TYPE(next, FILE_TYPE_SYMLINK) && !(state->mode & MODE_NOFOLLOW))
        {
            return path_symlink(irp, state, state->dentry);
        }
    }
}

static status_t path_verify(path_state_t* state, size_t length)
{
    state->mode = MODE_NONE;
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
    p++;

    while (true)
    {
        if (p < end && *p == '?')
        {
            memcpy(state->payload, p + 1, end - p - 1);
            state->payload[end - p - 1] = '\0';
            return OK;
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

        size_t length = p - token;
        mode_t mode = path_flag_to_mode(token, length);
        if (mode == MODE_NONE)
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

        size_t len = strnlen_s(dentry->name, MAX_NAME);
        if ((size_t)(ptr - pathname) < len + 1)
        {
            return ERR(VFS, NAMETOOLONG);
        }

        ptr -= len;
        memcpy(ptr, dentry->name, len);

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

status_t mode_to_string(mode_t mode, char* out, uint64_t length, uint64_t* outLength)
{
    if (out == NULL || length == 0)
    {
        return ERR(VFS, INVAL);
    }

    uint64_t index = 0;
    for (uint64_t i = 0; i < ARRAY_SIZE(flags); i++)
    {
        if (mode & flags[i].mode)
        {
            uint64_t nameLength = strnlen_s(flags[i].name, MAX_NAME);
            if (index + nameLength + 1 >= length)
            {
                return ERR(VFS, NAMETOOLONG);
            }

            out[index] = ':';
            index++;

            memcpy(&out[index], flags[i].name, nameLength);
            index += nameLength;
        }
    }

    out[index] = '\0';
    if (outLength != NULL)
    {
        *outLength = index;
    }
    return OK;
}

status_t mode_check(mode_t* mode, mode_t maxPerms)
{
    if (mode == NULL)
    {
        return ERR(VFS, INVAL);
    }

    if (((*mode & MODE_ALL_PERMS) & ~maxPerms) != MODE_NONE)
    {
        return ERR(VFS, ACCESS);
    }

    if ((*mode & MODE_ALL_PERMS) == MODE_NONE)
    {
        *mode |= maxPerms & MODE_ALL_PERMS;
    }

    return OK;
}