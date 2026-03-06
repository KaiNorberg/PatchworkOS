#include <_libstd/MAX_PATH.h>
#include <ctype.h>
#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/mutex.h>

#include <kernel/sync/rcu.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct path_flag_short
{
    mode_t mode;
} path_flag_short_t;

static path_flag_short_t shortFlags[UINT8_MAX + 1] = {
    ['r'] = {.mode = MODE_READ},
    ['w'] = {.mode = MODE_WRITE},
    ['x'] = {.mode = MODE_EXECUTE},
    ['a'] = {.mode = MODE_APPEND},
    ['f'] = {.mode = MODE_FILE},
    ['d'] = {.mode = MODE_DIRECTORY},
    ['s'] = {.mode = MODE_SYMLINK},
    ['h'] = {.mode = MODE_HARDLINK},
    ['e'] = {.mode = MODE_EXCLUSIVE},
    ['E'] = {.mode = MODE_EXISTING},
    ['t'] = {.mode = MODE_TRUNCATE},
    ['l'] = {.mode = MODE_NOFOLLOW},
    ['p'] = {.mode = MODE_PRIVATE},
    ['g'] = {.mode = MODE_PROPAGATE},
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
    {.mode = MODE_FILE, .name = "file"},
    {.mode = MODE_DIRECTORY, .name = "directory"},
    {.mode = MODE_SYMLINK, .name = "symlink"},
    {.mode = MODE_HARDLINK, .name = "hardlink"},
    {.mode = MODE_EXCLUSIVE, .name = "exclusive"},
    {.mode = MODE_EXISTING, .name = "existing"},
    {.mode = MODE_TRUNCATE, .name = "truncate"},
    {.mode = MODE_NOFOLLOW, .name = "nofollow"},
    {.mode = MODE_PRIVATE, .name = "private"},
    {.mode = MODE_PROPAGATE, .name = "propagate"},
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

static bool path_is_name_valid(const char* name)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return false;
    }

    for (uint64_t i = 0; i < MAX_NAME - 1; i++)
    {
        if (name[i] == '\0')
        {
            return true;
        }
        if (!path_is_char_valid(name[i]))
        {
            return false;
        }
    }

    return false;
}

static status_t path_walk_loop(irp_t* irp, path_state_t* state);

static void path_state_free(path_state_t* state)
{
    UNREF(state->ns);
    if (state->rootDentry != NULL)
    {
        UNREF(state->rootDentry);
    }
    if (state->rootBinding != NULL)
    {
        UNREF(state->rootBinding);
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
    /// @todo Implement system to only allow ".." if the current location can be reached from "root".

    status_t status = path_state_acquire(state);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    status = OK;
    uint64_t iter = 0;
    while (state->dentry == state->binding->source)
    {
        if (state->binding->parent == NULL || state->binding->target == NULL)
        {
            break;
        }

        binding_t* nextMount = REF(state->binding->parent);
        dentry_t* nextDentry = REF(state->binding->target);
        UNREF(state->binding);
        state->binding = nextMount;
        UNREF(state->dentry);
        state->dentry = nextDentry;

        iter++;
        if (iter >= PATH_MAX_DOTDOT)
        {
            status = ERR(VFS, LOOP);
            break;
        }
    }

    dentry_t* parent = REF(state->dentry->parent);
    UNREF(state->dentry);
    state->dentry = parent;

    if (IS_ERR(status))
    {
        path_state_free_acquired(state);
        return status;
    }
    path_state_release(state);
    return status;
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

    char* start = state->ptr - state->componentLen;
    size_t prefixLen = start - state->path;
    size_t suffixLen = (state->path + state->count) - state->ptr;
    size_t newLen = prefixLen + linkLen + suffixLen;

    if (newLen > MAX_PATH)
    {
        path_state_free_acquired(state);
        return ERR(VFS, PATHTOOLONG);
    }

    memmove(start + linkLen, state->ptr, suffixLen);
    memcpy(start, link, linkLen);

    if (state->payload != NULL)
    {
        state->payload = state->payload - state->componentLen + linkLen;
    }
    state->count = newLen;

    if (link[0] == '/')
    {
        state->ptr = state->path;
        path_t temp = {
            .dentry = state->dentry,
            .binding = state->binding,
        };
        namespace_get_root(state->ns, &temp);
        state->dentry = temp.dentry;
        state->binding = temp.binding;
    }
    else
    {
        state->ptr = start;
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

    status = mdl_add(mdl, NULL, state->linkBuffer, MAX_PATH);
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

static status_t path_lookup_complete(irp_t* irp, void* ctx)
{
    path_state_t* state = ctx;

    if (IS_ERR(irp->status))
    {
        path_state_free(state);
        return OK;
    }

    path_state_release(state);

    state->dentry = state->lookup;

    if (DENTRY_IS_SYMLINK(state->dentry) && !(state->mode & MODE_NOFOLLOW))
    {
        status_t status = path_symlink(irp, state, state->dentry);
        if (IS_ERR(status))
        {
            path_state_free(state);
            return status;
        }
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

    if (!DENTRY_IS_DIR(state->dentry))
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

    status_t status = state->done(irp, state, (file_t*)irp->result);
    path_state_free(ctx);
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

    rcu_read_unlock();

    file_t* file = file_new(dentry, mount, state->mode);
    if (file == NULL)
    {
        path_state_free(state);
        return ERR(VFS, NOMEM);
    }
    UNREF_DEFER(file);

    irp_prep_open(irp, state->payload);
    irp_set_complete(irp, path_done_complete, state);
    return file_call(file, irp);
}

static status_t path_walk_loop(irp_t* irp, path_state_t* state)
{
    if (state->ptr == state->path && state->ptr[0] == '/')
    {
        state->dentry = state->rootDentry;
        state->binding = state->rootBinding;
        state->ptr++;
    }

    while (true)
    {
        while (*state->ptr == '/')
        {
            state->ptr++;
        }

        if (*state->ptr == '\0' || *state->ptr == ':')
        {
            return path_done(irp, state);
        }

        const char* component = state->ptr;
        while (*state->ptr != '\0' && *state->ptr != '/' && *state->ptr != ':')
        {
            if (!path_is_char_valid(*state->ptr))
            {
                rcu_read_unlock();
                path_state_free(state);
                return ERR(VFS, INVALCHAR);
            }
            state->ptr++;
        }
        size_t len = state->ptr - component;
        state->componentLen = len;

        if (len == 1 && component[0] == '.')
        {
            continue;
        }
        if (len == 2 && component[0] == '.' && component[1] == '.')
        {
            status_t status = path_dotdot(state);
            if (IS_ERR(status))
            {
                return status;
            }

            continue;
        }

        dentry_t* next = dentry_rcu_get(state->dentry, component, len);
        if (next == NULL)
        {
            status_t status = path_walk_lookup(irp, state, component, len);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        if (atomic_load(&next->bindings) > 0)
        {
            namespace_rcu_traverse(state->ns, &state->binding, &next);
        }

        state->dentry = next;

        if (DENTRY_IS_SYMLINK(next) && !(state->mode & MODE_NOFOLLOW))
        {
            status_t status = path_symlink(irp, state, next);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }
}

static status_t path_verify(path_state_t* state)
{
    state->mode = MODE_NONE;
    state->payload = NULL;

    uint64_t index = 0;
    uint64_t currentNameLength = 0;
    while (state->path[index] != ':' && state->path[index] != '?')
    {
        if (index >= state->count)
        {
            return OK;
        }

        if (state->path[index] == '/')
        {
            currentNameLength = 0;
        }
        else
        {
            if (!path_is_char_valid(state->path[index]))
            {
                return ERR(VFS, INVALCHAR);
            }
            currentNameLength++;
            if (currentNameLength >= MAX_NAME)
            {
                return ERR(VFS, NAMETOOLONG);
            }
        }

        index++;
    }

    char delimiter = state->path[index];
    state->path[index] = '\0';
    index++;

    if (delimiter == '\0')
    {
        return OK;
    }

    if (delimiter == '?')
    {
        state->payload = &state->path[index];
        if (index < MAX_PATH)
        {
            state->path[state->count] = '\0';
        }
        return OK;
    }

    // Delimiter is ':'

    while (true)
    {
        while (state->path[index] == ':' && index < state->count)
        {
            index++;
        }

        if (state->path[index] == '?' || index >= state->count)
        {
            if (index < state->count && state->path[index] == '?')
            {
                state->path[index] = '\0';
                state->payload = &state->path[index + 1];
                if (index < MAX_PATH)
                {
                    state->path[state->count] = '\0';
                }
            }
            return OK;
        }

        const char* token = &state->path[index];
        while (state->path[index] != ':' && state->path[index] != '?' && index < state->count)
        {
            if (!isalpha(state->path[index]))
            {
                return ERR(VFS, INVALCHAR);
            }
            index++;
        }

        if (index >= state->count)
        {
            return OK;
        }

        size_t tokenLength = &state->path[index] - token;
        mode_t mode = path_flag_to_mode(token, tokenLength);
        if (mode == MODE_NONE)
        {
            return ERR(VFS, INVALFLAG);
        }

        state->mode |= mode;
        index++;
    }

    return OK;
}

status_t path_walk(irp_t* irp, path_state_t* state)
{
    if (state->ns == NULL)
    {
        state->ns = process_get_ns(irp_get_process(irp));
    }

    status_t status = path_verify(state);
    if (IS_ERR(status))
    {
        path_state_free(state);
        return status;
    }

    state->rootDentry = REF(state->rootDentry);
    state->rootBinding = REF(state->rootBinding);

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