#include <_internal/MAX_PATH.h>
#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/mutex.h>

#include <errno.h>
#include <kernel/utils/map.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static map_t flagMap;
static map_t flagShortMap;

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
    ['t'] = {.mode = MODE_TRUNCATE},
    ['d'] = {.mode = MODE_DIRECTORY},
    ['R'] = {.mode = MODE_RECURSIVE},
    ['f'] = {.mode = MODE_NOFOLLOW},
    ['i'] = {.mode = MODE_NOINHERIT},
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
    {.mode = MODE_TRUNCATE, .name = "truncate"},
    {.mode = MODE_DIRECTORY, .name = "directory"},
    {.mode = MODE_RECURSIVE, .name = "recursive"},
    {.mode = MODE_NOFOLLOW, .name = "nofollow"},
    {.mode = MODE_NOINHERIT, .name = "noinherit"},
};

static mode_t path_flag_to_mode(const char* flag, uint64_t length)
{
    if (flag == NULL || length == 0)
    {
        return MODE_NONE;
    }

    for (uint64_t i = 0; i < MODE_AMOUNT; i++)
    {
        size_t len = strnlen_s(flags[i].name, MAX_NAME);
        if (len == length && strncmp(flag, flags[i].name, length) == 0)
        {
            return flags[i].mode;
        }
    }

    mode_t combinedMode = MODE_NONE;
    for (uint64_t i = 0; i < length; i++)
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

static uint64_t path_handle_dotdot(path_t* path)
{
    if (path->dentry == path->mount->source)
    {
        uint64_t iter = 0;
        while (path->dentry == path->mount->source && iter < PATH_MAX_DOTDOT)
        {
            if (path->mount->parent == NULL || path->mount->target == NULL)
            {
                return 0;
            }

            path_set(path, path->mount->parent, path->mount->target);
            iter++;
        }

        if (iter >= PATH_MAX_DOTDOT)
        {
            errno = ELOOP;
            return ERR;
        }

        if (path->dentry != path->mount->source)
        {
            path_set(path, path->mount, path->dentry->parent);
        }

        return 0;
    }

    path_set(path, path->mount, path->dentry->parent);
    return 0;
}

static uint64_t path_walk_depth(path_t* path, const pathname_t* pathname, namespace_handle_t* ns, uint64_t symlinks);

static uint64_t path_follow_symlink(dentry_t* dentry, path_t* path, namespace_handle_t* ns, uint64_t symlinks)
{
    if (symlinks >= PATH_MAX_SYMLINK)
    {
        errno = ELOOP;
        return ERR;
    }

    char symlinkPath[MAX_PATH];
    if (vfs_readlink(dentry->inode, symlinkPath, MAX_PATH) == ERR)
    {
        return ERR;
    }

    pathname_t pathname;
    if (pathname_init(&pathname, symlinkPath) == ERR)
    {
        return ERR;
    }

    if (path_walk_depth(path, &pathname, ns, symlinks + 1) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t path_step_depth(path_t* path, mode_t mode, const char* name, namespace_handle_t* ns, uint64_t symlinks)
{
    namespace_traverse(ns, path);

    dentry_t* next = dentry_lookup(path, name);
    if (next == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(next);

    if (DENTRY_IS_SYMLINK(next) && !(mode & MODE_NOFOLLOW))
    {
        if (path_follow_symlink(next, path, ns, symlinks) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        path_set(path, path->mount, next);
    }

    namespace_traverse(ns, path);

    return 0;
}

uint64_t path_step(path_t* path, mode_t mode, const char* name, namespace_handle_t* ns)
{
    if (path == NULL || name == NULL || !path_is_name_valid(name) || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return path_step_depth(path, mode, name, ns, 0);
}

static uint64_t path_walk_depth(path_t* path, const pathname_t* pathname, namespace_handle_t* ns, uint64_t symlinks)
{
    if (path == NULL || !PATHNAME_IS_VALID(pathname) || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    const char* p = pathname->string;
    if (pathname->string[0] == '/')
    {
        if (namespace_get_root_path(ns, path) == ERR)
        {
            return ERR;
        }
        p++;
    }

    assert(path->dentry != NULL && path->mount != NULL);

    char component[MAX_NAME];
    while (*p != '\0')
    {
        while (*p == '/')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        const char* start = p;
        while (*p != '\0' && *p != '/')
        {
            p++;
        }

        uint64_t len = p - start;
        if (len >= MAX_NAME)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }

        memcpy(component, start, len);
        component[len] = '\0';

        if (len == 1 && component[0] == '.')
        {
            continue;
        }

        if (len == 2 && component[0] == '.' && component[1] == '.')
        {
            if (path_handle_dotdot(path) == ERR)
            {
                return ERR;
            }
            continue;
        }

        if (path_step_depth(path, pathname->mode, component, ns, symlinks) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t path_walk(path_t* path, const pathname_t* pathname, namespace_handle_t* ns)
{
    return path_walk_depth(path, pathname, ns, 0);
}

uint64_t path_walk_parent(path_t* path, const pathname_t* pathname, char* outLastName, namespace_handle_t* ns)
{
    if (path == NULL || !PATHNAME_IS_VALID(pathname) || outLastName == NULL || ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(outLastName, 0, MAX_NAME);

    if (strcmp(pathname->string, "/") == 0)
    {
        errno = ENOENT;
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
    namespace_handle_t* ns)
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

    memset(pathname->string, 0, MAX_PATH);
    pathname->mode = MODE_NONE;
    pathname->isValid = false;

    path_t current = PATH_CREATE(path->mount, path->dentry);
    PATH_DEFER(&current);

    uint64_t index = MAX_PATH - 1;
    pathname->string[index] = '\0';

    while (true)
    {
        if (DENTRY_IS_ROOT(current.dentry))
        {
            if (current.mount->parent == NULL)
            {
                pathname->string[index] = '/';
                break;
            }
            path_set(&current, current.mount->parent, current.mount->target);
            continue;
        }

        uint64_t nameLength = strnlen_s(current.dentry->name, MAX_NAME);

        if (index < nameLength + 1)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }

        if (nameLength != 0)
        {
            index -= nameLength;
            memcpy(pathname->string + index, current.dentry->name, nameLength);

            index--;
            pathname->string[index] = '/';
        }

        path_set(&current, current.mount, current.dentry->parent);
    }

    uint64_t length = MAX_PATH - index;
    memmove(pathname->string, pathname->string + index, length);
    pathname->string[length] = '\0';
    pathname->isValid = true;
    return 0;
}
