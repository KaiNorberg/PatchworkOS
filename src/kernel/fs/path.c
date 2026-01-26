#include <_libstd/MAX_PATH.h>
#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
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

status_t pathname_init(pathname_t* pathname, const char* string)
{
    if (pathname == NULL || string == NULL)
    {
        return ERR(VFS, INVAL);
    }

    memset(pathname->string, 0, MAX_PATH);
    pathname->mode = MODE_NONE;

    uint64_t length = strnlen_s(string, MAX_PATH);
    if (length >= MAX_PATH)
    {
        return ERR(VFS, PATHTOOLONG);
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
                return ERR(VFS, INVALCHAR);
            }
            currentNameLength++;
            if (currentNameLength >= MAX_NAME)
            {
                return ERR(VFS, NAMETOOLONG);
            }
        }

        pathname->string[index] = string[index];
        index++;
    }

    pathname->string[index] = '\0';

    if (string[index] != ':')
    {
        return OK;
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
                return ERR(VFS, INVALCHAR);
            }
            index++;
        }

        uint64_t tokenLength = &string[index] - token;
        if (tokenLength >= MAX_NAME)
        {
            return ERR(VFS, NAMETOOLONG);
        }

        mode_t mode = path_flag_to_mode(token, tokenLength);
        if (mode == MODE_NONE)
        {
            return ERR(VFS, INVALFLAG);
        }

        pathname->mode |= mode;
    }

    return OK;
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

static status_t path_rcu_walk(path_walk_ctx_t* ctx);

static inline status_t path_walk_acquire(path_walk_ctx_t* ctx)
{
    if (REF_TRY(ctx->dentry) == NULL)
    {
        return ERR(VFS, NOENT);
    }
    if (REF_TRY(ctx->mount) == NULL)
    {
        UNREF(ctx->dentry);
        return ERR(VFS, NOENT);
    }

    rcu_read_unlock();
    return OK;
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

static inline status_t path_walk_get_result(path_walk_ctx_t* ctx, path_t* path)
{
    if (ctx->mount != NULL && REF_TRY(ctx->mount) == NULL)
    {
        return ERR(VFS, NOENT);
    }

    if ((ctx->lookup == NULL || ctx->dentry != ctx->lookup) && ctx->dentry != NULL && REF_TRY(ctx->dentry) == NULL)
    {
        UNREF(ctx->mount);
        return ERR(VFS, NOENT);
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

    return OK;
}

static status_t path_rcu_dotdot(path_walk_ctx_t* ctx)
{
    status_t status = path_walk_acquire(ctx);
    if (!IS_OK(status))
    {
        return status;
    }

    status = OK;
    uint64_t iter = 0;
    while (ctx->dentry == ctx->mount->source)
    {
        if (ctx->mount->parent == NULL || ctx->mount->target == NULL)
        {
            break;
        }

        mount_t* nextMount = REF(ctx->mount->parent);
        dentry_t* nextDentry = REF(ctx->mount->target);
        UNREF(ctx->mount);
        ctx->mount = nextMount;
        UNREF(ctx->dentry);
        ctx->dentry = nextDentry;

        iter++;
        if (iter >= PATH_MAX_DOTDOT)
        {
            status = ERR(VFS, LOOP);
            break;
        }
    }

    dentry_t* parent = REF(ctx->dentry->parent);
    UNREF(ctx->dentry);
    ctx->dentry = parent;

    path_walk_release(ctx);
    return status;
}

static status_t path_rcu_symlink(path_walk_ctx_t* ctx, dentry_t* symlink)
{
    if (ctx->symlinks >= PATH_MAX_SYMLINK)
    {
        return ERR(VFS, LOOP);
    }

    if (REF_TRY(symlink) == NULL)
    {
        return ERR(VFS, NOENT);
    }
    UNREF_DEFER(symlink);

    status_t status = path_walk_acquire(ctx);
    if (!IS_OK(status))
    {
        return status;
    }

    char symlinkPath[MAX_PATH];
    size_t readCount;
    status = vfs_readlink(symlink->vnode, symlinkPath, MAX_PATH - 1, &readCount);

    path_walk_release(ctx);

    if (!IS_OK(status))
    {
        return status;
    }
    symlinkPath[readCount] = '\0';

    pathname_t pathname;
    status = pathname_init(&pathname, symlinkPath);
    if (!IS_OK(status))
    {
        return status;
    }

    const pathname_t* oldPathname = ctx->pathname;

    ctx->pathname = &pathname;
    ctx->symlinks++;

    status = path_rcu_walk(ctx);

    ctx->symlinks--;
    ctx->pathname = oldPathname;

    return status;
}

static status_t path_rcu_step(path_walk_ctx_t* ctx, const char* name, size_t length)
{
    if (length == 1 && name[0] == '.')
    {
        return OK;
    }

    if (length == 2 && name[0] == '.' && name[1] == '.')
    {
        return path_rcu_dotdot(ctx);
    }

    dentry_t* next = dentry_rcu_get(ctx->dentry, name, length);
    if (next == NULL)
    {
        status_t status = path_walk_acquire(ctx);
        if (IS_ERR(status))
        {
            return status;
        }

        status = dentry_lookup(&next, ctx->dentry, name, length);

        path_walk_release(ctx);

        if (IS_ERR(status))
        {
            return status;
        }

        path_walk_set_lookup(ctx, next);
    }

    if (DENTRY_IS_SYMLINK(next) && !(ctx->mode & MODE_NOFOLLOW))
    {
        status_t status = path_rcu_symlink(ctx, next);
        if (IS_ERR(status))
        {
            return status;
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

    return OK;
}

static status_t path_rcu_walk(path_walk_ctx_t* ctx)
{
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

        status_t status = path_rcu_step(ctx, component, length);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    return OK;
}

status_t path_step(path_t* path, mode_t mode, const char* name, namespace_t* ns)
{
    if (path == NULL || name == NULL || !path_is_name_valid(name) || ns == NULL)
    {
        return ERR(VFS, INVAL);
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

    status_t status = path_rcu_step(&ctx, name, strlen(name));
    if (IS_ERR(status))
    {
        path_walk_cleanup(&ctx);
        return status;
    }

    status = path_walk_get_result(&ctx, path);
    if (IS_ERR(status))
    {
        path_walk_cleanup(&ctx);
        return status;
    }

    return OK;
}

status_t path_walk(path_t* path, const pathname_t* pathname, namespace_t* ns)
{
    if (path == NULL || pathname == NULL || ns == NULL)
    {
        return ERR(VFS, INVAL);
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

    status_t status = path_rcu_walk(&ctx);
    if (IS_ERR(status))
    {
        path_walk_cleanup(&ctx);
        return status;
    }

    status = path_walk_get_result(&ctx, path);
    if (IS_ERR(status))
    {
        path_walk_cleanup(&ctx);
        return status;
    }

    return OK;
}

status_t path_walk_parent(path_t* path, const pathname_t* pathname, char* outLastName, namespace_t* ns)
{
    if (path == NULL || pathname == NULL || outLastName == NULL || ns == NULL)
    {
        return ERR(VFS, INVAL);
    }

    memset(outLastName, 0, MAX_NAME);

    if (strcmp(pathname->string, "/") == 0)
    {
        return ERR(VFS, INVAL);
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
        return OK;
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
    status_t status = pathname_init(&parentPathname, string);
    if (IS_ERR(status))
    {
        return status;
    }

    return path_walk(path, &parentPathname, ns);
}

status_t path_walk_parent_and_child(const path_t* from, path_t* outParent, path_t* outChild, const pathname_t* pathname,
    namespace_t* ns)
{
    if (from == NULL || outParent == NULL || outChild == NULL || pathname == NULL || ns == NULL)
    {
        return ERR(VFS, INVAL);
    }

    char lastName[MAX_NAME];
    path_copy(outParent, from);
    status_t status = path_walk_parent(outParent, pathname, lastName, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    path_copy(outChild, outParent);
    status = path_step(outChild, pathname->mode, lastName, ns);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

status_t path_to_name(const path_t* path, pathname_t* pathname)
{
    if (path == NULL || path->dentry == NULL || path->mount == NULL || pathname == NULL)
    {
        return ERR(VFS, INVAL);
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
            return ERR(VFS, NOENT);
        }

        size_t len = strnlen_s(dentry->name, MAX_NAME);
        if ((size_t)(ptr - buffer) < len + 1)
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
        if (ptr == buffer)
        {
            return ERR(VFS, NAMETOOLONG);
        }
        ptr--;
        *ptr = '/';
    }

    size_t totalLen = (buffer + MAX_PATH - 1) - ptr;
    memmove(buffer, ptr, totalLen + 1);

    pathname->mode = MODE_NONE;
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

#ifdef _TESTING_

#include <kernel/utils/test.h>

TEST_DEFINE(path)
{
    pathname_t pathname;

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "/usr/bin/init")));
    TEST_ASSERT(strcmp(pathname.string, "/usr/bin/init") == 0);
    TEST_ASSERT(pathname.mode == MODE_NONE);

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "/dev/sda:read:write")));
    TEST_ASSERT(strcmp(pathname.string, "/dev/sda") == 0);
    TEST_ASSERT((pathname.mode & (MODE_READ | MODE_WRITE)) == (MODE_READ | MODE_WRITE));

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "/tmp/file:c:w")));
    TEST_ASSERT(strcmp(pathname.string, "/tmp/file") == 0);
    TEST_ASSERT((pathname.mode & (MODE_CREATE | MODE_WRITE)) == (MODE_CREATE | MODE_WRITE));

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "/var/log:append:c")));
    TEST_ASSERT(strcmp(pathname.string, "/var/log") == 0);
    TEST_ASSERT((pathname.mode & (MODE_APPEND | MODE_CREATE)) == (MODE_APPEND | MODE_CREATE));

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "/file:rw")));
    TEST_ASSERT(strcmp(pathname.string, "/file") == 0);
    TEST_ASSERT((pathname.mode & (MODE_READ | MODE_WRITE)) == (MODE_READ | MODE_WRITE));

    TEST_ASSERT(IS_CODE(pathname_init(&pathname, "/home/user/fi?le"), INVALCHAR));

    TEST_ASSERT(IS_CODE(pathname_init(&pathname, "/home:invalid"), INVALFLAG));

    TEST_ASSERT(IS_OK(pathname_init(&pathname, "")));
    TEST_ASSERT(strcmp(pathname.string, "") == 0);

    TEST_ASSERT(IS_OK(pathname_init(&pathname, ":read")));
    TEST_ASSERT(strcmp(pathname.string, "") == 0);
    TEST_ASSERT(pathname.mode == MODE_READ);

    return 0;
}

#endif