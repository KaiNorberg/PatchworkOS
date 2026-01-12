#include <_internal/MAX_PATH.h>
#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/mutex.h>

#include <errno.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/map.h>
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
    ['n'] = {.mode = MODE_NONBLOCK},
    ['a'] = {.mode = MODE_APPEND},
    ['c'] = {.mode = MODE_CREATE},
    ['e'] = {.mode = MODE_EXCLUSIVE},
    ['p'] = {.mode = MODE_PARENTS},
    ['t'] = {.mode = MODE_TRUNCATE},
    ['d'] = {.mode = MODE_DIRECTORY},
    ['R'] = {.mode = MODE_RECURSIVE},
    ['l'] = {.mode = MODE_NOFOLLOW},
    ['P'] = {.mode = MODE_PRIVATE},
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
    {.mode = MODE_NONBLOCK, .name = "nonblock"},
    {.mode = MODE_APPEND, .name = "append"},
    {.mode = MODE_CREATE, .name = "create"},
    {.mode = MODE_EXCLUSIVE, .name = "exclusive"},
    {.mode = MODE_PARENTS, .name = "parents"},
    {.mode = MODE_TRUNCATE, .name = "truncate"},
    {.mode = MODE_DIRECTORY, .name = "directory"},
    {.mode = MODE_RECURSIVE, .name = "recursive"},
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

uint64_t pathname_init(pathname_t* pathname, const char* string)
{
    if (pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(pathname->string, 0, MAX_PATH);
    pathname->mode = MODE_NONE;
    pathname->isValid = false;

    if (string == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t length = strnlen_s(string, MAX_PATH);
    if (length >= MAX_PATH)
    {
        errno = ENAMETOOLONG;
        return ERR;
    }

    uint64_t index = 0;
    uint64_t currentNameLength = 0;
    while (string[index] != '\0' && string[index] != ':')
    {
        if (string[index] == '/')
        {
            currentNameLength = 0;
        }
        else
        {
            if (!path_is_char_valid(string[index]))
            {
                errno = EINVAL;
                return ERR;
            }
            currentNameLength++;
            if (currentNameLength >= MAX_NAME)
            {
                errno = ENAMETOOLONG;
                return ERR;
            }
        }

        pathname->string[index] = string[index];
        index++;
    }

    pathname->string[index] = '\0';

    if (string[index] != ':')
    {
        pathname->isValid = true;
        return 0;
    }

    index++; // Skip ':'.
    const char* flags = &string[index];

    while (true)
    {
        while (string[index] == ':')
        {
            index++;
        }

        if (string[index] == '\0')
        {
            break;
        }

        const char* token = &string[index];
        while (string[index] != '\0' && string[index] != ':')
        {
            if (!isalnum(string[index]))
            {
                errno = EINVAL;
                return ERR;
            }
            index++;
        }

        uint64_t tokenLength = &string[index] - token;
        if (tokenLength >= MAX_NAME)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }

        mode_t mode = path_flag_to_mode(token, tokenLength);
        if (mode == MODE_NONE)
        {
            errno = EINVAL;
            return ERR;
        }

        pathname->mode |= mode;
    }

    pathname->isValid = true;
    return 0;
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

void path_set(path_t* path, mount_t* mount, dentry_t* dentry)
{
    if (dentry != NULL)
    {
        REF(dentry);
    }

    if (mount != NULL)
    {
        REF(mount);
    }

    if (path->dentry != NULL)
    {
        UNREF(path->dentry);
    }

    if (path->mount != NULL)
    {
        UNREF(path->mount);
    }

    path->dentry = dentry;
    path->mount = mount;
}

void path_copy(path_t* dest, const path_t* src)
{
    path_set(dest, src->mount, src->dentry);
}

void path_put(path_t* path)
{
    if (path->dentry != NULL)
    {
        UNREF(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        UNREF(path->mount);
        path->mount = NULL;
    }
}

typedef struct
{
    const pathname_t* pathname;
    namespace_t* ns;
    mode_t mode;
    mount_t* mount;
    dentry_t* dentry;
    uint64_t symlinks;
    dentry_t* lookup;
} path_walk_ctx_t;

static uint64_t path_rcu_walk(path_walk_ctx_t* ctx);

static inline uint64_t path_walk_acquire(path_walk_ctx_t* ctx)
{
    if (REF_TRY(ctx->dentry) == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    if (REF_TRY(ctx->mount) == NULL)
    {
        UNREF(ctx->dentry);
        errno = ENOENT;
        return ERR;
    }

    rcu_read_unlock();
    return 0;
}

static inline void path_walk_release(path_walk_ctx_t* ctx)
{
    rcu_read_lock();

    UNREF(ctx->dentry);
    UNREF(ctx->mount);
}

static inline void path_walk_set_lookup(path_walk_ctx_t* ctx, dentry_t* dentry)
{
    if (ctx->lookup != NULL)
    {
        UNREF(ctx->lookup);
    }
    ctx->lookup = dentry;
}

static inline void path_walk_cleanup(path_walk_ctx_t* ctx)
{
    if (ctx->lookup != NULL)
    {
        UNREF(ctx->lookup);
        ctx->lookup = NULL;
    }
}

static inline uint64_t path_walk_get_result(path_walk_ctx_t* ctx, path_t* path)
{
    if (ctx->mount != NULL && REF_TRY(ctx->mount) == NULL)
    {
        errno = ENONET;
        return ERR;
    }

    if ((ctx->lookup == NULL || ctx->dentry != ctx->lookup) && ctx->dentry != NULL && REF_TRY(ctx->dentry) == NULL)
    {
        UNREF(ctx->mount);
        errno = ENOENT;
        return ERR;
    }

    UNREF(path->mount);
    UNREF(path->dentry);
    path->mount = ctx->mount;
    path->dentry = ctx->dentry;

    if (ctx->lookup != NULL && ctx->dentry != ctx->lookup)
    {
        UNREF(ctx->lookup);
    }
    ctx->lookup = NULL;

    return 0;
}

static uint64_t path_rcu_dotdot(path_walk_ctx_t* ctx)
{
    if (path_walk_acquire(ctx) == ERR)
    {
        return ERR;
    }

    uint64_t result = 0;

    uint64_t iter = 0;
    while (ctx->dentry == ctx->mount->source)
    {
        if (ctx->mount->parent == NULL || ctx->mount->target == NULL)
        {
            break;
        }

        REF(ctx->mount->parent);
        REF(ctx->mount->target);

        UNREF(ctx->mount);
        ctx->mount = ctx->mount->parent;
        UNREF(ctx->dentry);
        ctx->dentry = ctx->mount->target;

        iter++;
        if (iter >= PATH_MAX_DOTDOT)
        {
            errno = ELOOP;
            result = ERR;
            break;
        }
    }

    dentry_t* parent = REF(ctx->dentry->parent);
    UNREF(ctx->dentry);
    ctx->dentry = parent;

    path_walk_release(ctx);
    return result;
}

static uint64_t path_rcu_symlink(path_walk_ctx_t* ctx, dentry_t* symlink)
{
    if (ctx->symlinks >= PATH_MAX_SYMLINK)
    {
        errno = ELOOP;
        return ERR;
    }

    if (REF_TRY(symlink) == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(symlink);

    if (path_walk_acquire(ctx) == ERR)
    {
        return ERR;
    }

    char symlinkPath[MAX_PATH];
    size_t readCount = vfs_readlink(symlink->inode, symlinkPath, MAX_PATH - 1);

    path_walk_release(ctx);

    if (readCount == ERR)
    {
        return ERR;
    }
    symlinkPath[readCount] = '\0';

    pathname_t pathname;
    if (pathname_init(&pathname, symlinkPath) == ERR)
    {
        return ERR;
    }

    const pathname_t* oldPathname = ctx->pathname;

    ctx->pathname = &pathname;
    ctx->symlinks++;

    uint64_t result = path_rcu_walk(ctx);

    ctx->symlinks--;
    ctx->pathname = oldPathname;

    return result;
}

static uint64_t path_rcu_step(path_walk_ctx_t* ctx, const char* name, size_t length)
{
    if (length == 1 && name[0] == '.')
    {
        return 0;
    }

    if (length == 2 && name[0] == '.' && name[1] == '.')
    {
        return path_rcu_dotdot(ctx);
    }

    dentry_t* next = dentry_rcu_get(ctx->dentry, name, length);
    if (next == NULL)
    {
        if (path_walk_acquire(ctx) == ERR)
        {
            return ERR;
        }

        next = dentry_lookup(ctx->dentry, name, length);

        path_walk_release(ctx);

        if (next == NULL)
        {
            return ERR;
        }

        path_walk_set_lookup(ctx, next);
    }

    if (DENTRY_IS_SYMLINK(next) && !(ctx->mode & MODE_NOFOLLOW))
    {
        if (path_rcu_symlink(ctx, next) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        ctx->dentry = next;
    }

    if (atomic_load(&next->mountCount) != 0)
    {
        namespace_rcu_traverse(ctx->ns, &ctx->mount, &ctx->dentry);
    }

    return 0;
}

static uint64_t path_rcu_walk(path_walk_ctx_t* ctx)
{
    RCU_READ_SCOPE();

    const char* p = ctx->pathname->string;
    if (ctx->pathname->string[0] == '/')
    {
        namespace_rcu_get_root(ctx->ns, &ctx->mount, &ctx->dentry);
        p++;
    }

    while (true)
    {
        while (*p == '/')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        const char* component = p;
        while (*p != '\0' && *p != '/')
        {
            p++;
        }
        size_t length = p - component;

        if (path_rcu_step(ctx, component, length) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t path_step(path_t* path, mode_t mode, const char* name, namespace_t* ns)
{
    if (path == NULL || name == NULL || !path_is_name_valid(name) || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RCU_READ_SCOPE();

    path_walk_ctx_t ctx = {
        .pathname = NULL,
        .ns = ns,
        .mode = mode,
        .mount = path->mount,
        .dentry = path->dentry,
        .symlinks = 0,
        .lookup = NULL,
    };

    if (path->dentry != NULL && atomic_load(&path->dentry->mountCount) != 0)
    {
        namespace_rcu_traverse(ctx.ns, &ctx.mount, &ctx.dentry);
    }

    if (path_rcu_step(&ctx, name, strlen(name)) == ERR)
    {
        path_walk_cleanup(&ctx);
        return ERR;
    }

    if (path_walk_get_result(&ctx, path) == ERR)
    {
        path_walk_cleanup(&ctx);
        return ERR;
    }

    return 0;
}

uint64_t path_walk(path_t* path, const pathname_t* pathname, namespace_t* ns)
{
    if (path == NULL || !PATHNAME_IS_VALID(pathname) || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RCU_READ_SCOPE();

    path_walk_ctx_t ctx = {
        .pathname = pathname,
        .ns = ns,
        .mode = pathname->mode,
        .mount = path->mount,
        .dentry = path->dentry,
        .symlinks = 0,
        .lookup = NULL,
    };

    if (path->dentry != NULL && atomic_load(&path->dentry->mountCount) != 0)
    {
        namespace_rcu_traverse(ctx.ns, &ctx.mount, &ctx.dentry);
    }

    if (path_rcu_walk(&ctx) == ERR)
    {
        path_walk_cleanup(&ctx);
        return ERR;
    }

    if (path_walk_get_result(&ctx, path) == ERR)
    {
        path_walk_cleanup(&ctx);
        return ERR;
    }

    return 0;
}

uint64_t path_walk_parent(path_t* path, const pathname_t* pathname, char* outLastName, namespace_t* ns)
{
    if (path == NULL || !PATHNAME_IS_VALID(pathname) || outLastName == NULL || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(outLastName, 0, MAX_NAME);

    if (strcmp(pathname->string, "/") == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    char string[MAX_PATH];
    strncpy(string, pathname->string, MAX_PATH - 1);
    string[MAX_PATH - 1] = '\0';

    uint64_t len = strnlen_s(string, MAX_PATH);
    while (len > 1 && string[len - 1] == '/')
    {
        string[len - 1] = '\0';
        len--;
    }

    char* lastSlash = strrchr(string, '/');
    if (lastSlash == NULL)
    {
        strncpy(outLastName, string, MAX_NAME - 1);
        outLastName[MAX_NAME - 1] = '\0';
        return 0;
    }

    char* lastComponent = lastSlash + 1;
    strncpy(outLastName, lastComponent, MAX_NAME - 1);
    outLastName[MAX_NAME - 1] = '\0';

    if (lastSlash == string)
    {
        string[1] = '\0';
    }
    else
    {
        *lastSlash = '\0';
    }

    pathname_t parentPathname;
    if (pathname_init(&parentPathname, string) == ERR)
    {
        return ERR;
    }

    return path_walk(path, &parentPathname, ns);
}

uint64_t path_walk_parent_and_child(const path_t* from, path_t* outParent, path_t* outChild, const pathname_t* pathname,
    namespace_t* ns)
{
    if (from == NULL || outParent == NULL || outChild == NULL || !PATHNAME_IS_VALID(pathname) || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char lastName[MAX_NAME];
    path_copy(outParent, from);
    if (path_walk_parent(outParent, pathname, lastName, ns) == ERR)
    {
        return ERR;
    }

    path_copy(outChild, outParent);
    if (path_step(outChild, pathname->mode, lastName, ns) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t path_to_name(const path_t* path, pathname_t* pathname)
{
    if (path == NULL || path->dentry == NULL || path->mount == NULL || pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char* buffer = pathname->string;
    char* ptr = buffer + MAX_PATH - 1;
    *ptr = '\0';

    dentry_t* dentry = path->dentry;
    mount_t* mount = path->mount;

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
            errno = ENOENT;
            return ERR;
        }

        size_t len = strnlen_s(dentry->name, MAX_NAME);
        if ((size_t)(ptr - buffer) < len + 1)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }

        ptr -= len;
        memcpy(ptr, dentry->name, len);

        ptr--;
        *ptr = '/';

        dentry = dentry->parent;
    }

    if (*ptr == '\0')
    {
        if (ptr == buffer)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }
        ptr--;
        *ptr = '/';
    }

    size_t totalLen = (buffer + MAX_PATH - 1) - ptr;
    memmove(buffer, ptr, totalLen + 1);

    pathname->mode = MODE_NONE;
    pathname->isValid = true;

    return 0;
}

uint64_t mode_to_string(mode_t mode, char* out, uint64_t length)
{
    if (out == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t index = 0;
    for (uint64_t i = 0; i < ARRAY_SIZE(flags); i++)
    {
        if (mode & flags[i].mode)
        {
            uint64_t nameLength = strnlen_s(flags[i].name, MAX_NAME);
            if (index + nameLength + 1 >= length)
            {
                errno = ENAMETOOLONG;
                return ERR;
            }

            out[index] = ':';
            index++;

            memcpy(&out[index], flags[i].name, nameLength);
            index += nameLength;
        }
    }

    out[index] = '\0';
    return index;
}

uint64_t mode_check(mode_t* mode, mode_t maxPerms)
{
    if (mode == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (((*mode & MODE_ALL_PERMS) & ~maxPerms) != MODE_NONE)
    {
        errno = EACCES;
        return ERR;
    }

    if ((*mode & MODE_ALL_PERMS) == MODE_NONE)
    {
        *mode |= maxPerms & MODE_ALL_PERMS;
    }

    return 0;
}