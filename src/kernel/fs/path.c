#include <kernel/fs/path.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/mutex.h>

#include <errno.h>
#include <string.h>

static map_t flagMap;
static map_t flagShortMap;

typedef struct path_flag_entry
{
    map_entry_t entry;
    map_entry_t shortEntry;
    path_flags_t flag;
    const char* name;
} path_flag_entry_t;

static path_flag_entry_t flagEntries[] = {
    {.flag = PATH_NONBLOCK, .name = "nonblock"},
    {.flag = PATH_APPEND, .name = "append"},
    {.flag = PATH_CREATE, .name = "create"},
    {.flag = PATH_EXCLUSIVE, .name = "excl"},
    {.flag = PATH_TRUNCATE, .name = "trunc"},
    {.flag = PATH_DIRECTORY, .name = "dir"},
    {.flag = PATH_RECURSIVE, .name = "recur"},
};

static path_flag_entry_t* path_flags_get(const char* flag, uint64_t length)
{
    if (flag == NULL || length == 0)
    {
        return NULL;
    }

    assert(sizeof(flagEntries) / sizeof(flagEntries[0]) == PATH_FLAGS_AMOUNT);

    map_key_t key = map_key_buffer(flag, length);
    if (length == 1)
    {
        path_flag_entry_t* entry = CONTAINER_OF_SAFE(map_get(&flagShortMap, &key), path_flag_entry_t, shortEntry);
        if (entry == NULL)
        {
            errno = EBADFLAG;
            return NULL;
        }

        return entry;
    }

    path_flag_entry_t* entry = CONTAINER_OF_SAFE(map_get(&flagMap, &key), path_flag_entry_t, entry);
    if (entry == NULL)
    {
        errno = EBADFLAG;
        return NULL;
    }

    return entry;
}

void path_flags_init(void)
{
    map_init(&flagMap);
    map_init(&flagShortMap);

    for (uint64_t i = 0; i < PATH_FLAGS_AMOUNT; i++)
    {
        map_entry_init(&flagEntries[i].entry);
        map_entry_init(&flagEntries[i].shortEntry);

#ifndef NDEBUG
        for (uint64_t j = 0; j < PATH_FLAGS_AMOUNT; j++)
        {
            if (i != j && flagEntries[i].name[0] == flagEntries[j].name[0])
            {
                panic(NULL, "Flag name collision");
            }
        }
#endif

        map_key_t key = map_key_string(flagEntries[i].name);
        if (map_insert(&flagMap, &key, &flagEntries[i].entry) == ERR)
        {
            panic(NULL, "Failed to init flag map");
        }

        map_key_t shortKey = map_key_buffer(flagEntries[i].name, 1);
        if (map_insert(&flagShortMap, &shortKey, &flagEntries[i].shortEntry) == ERR)
        {
            panic(NULL, "Failed to init short flag map");
        }
    }
}

uint64_t pathname_init(pathname_t* pathname, const char* string)
{
    if (pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(pathname->string, 0, MAX_PATH);
    pathname->flags = PATH_NONE;
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
            if (!PATH_VALID_CHAR(string[index]))
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
                errno = EBADFLAG;
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

        path_flag_entry_t* flag = path_flags_get(token, tokenLength);
        if (flag == NULL)
        {
            errno = EBADFLAG;
            return ERR;
        }

        pathname->flags |= flag->flag;
    }

    pathname->isValid = true;
    return 0;
}

void path_set(path_t* path, mount_t* mount, dentry_t* dentry)
{
    if (path->dentry != NULL)
    {
        DEREF(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        DEREF(path->mount);
        path->mount = NULL;
    }

    if (dentry != NULL)
    {
        path->dentry = REF(dentry);
    }

    if (mount != NULL)
    {
        path->mount = REF(mount);
    }
}

void path_copy(path_t* dest, const path_t* src)
{
    if (dest->dentry != NULL)
    {
        DEREF(dest->dentry);
        dest->dentry = NULL;
    }

    if (dest->mount != NULL)
    {
        DEREF(dest->mount);
        dest->mount = NULL;
    }

    if (src->dentry != NULL)
    {
        dest->dentry = REF(src->dentry);
    }

    if (src->mount != NULL)
    {
        dest->mount = REF(src->mount);
    }
}

void path_put(path_t* path)
{
    if (path->dentry != NULL)
    {
        DEREF(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        DEREF(path->mount);
        path->mount = NULL;
    }
}

static uint64_t path_handle_dotdot(path_t* current)
{
    if (current->dentry == current->mount->root)
    {
        uint64_t iter = 0;

        while (current->dentry == current->mount->root && iter < PATH_HANDLE_DOTDOT_MAX_ITER)
        {
            if (current->mount->parent == NULL || current->mount->mountpoint == NULL)
            {
                return 0;
            }

            mount_t* newMount = REF(current->mount->parent);
            dentry_t* newDentry = REF(current->mount->mountpoint);

            DEREF(current->mount);
            current->mount = newMount;
            DEREF(current->dentry);
            current->dentry = newDentry;

            iter++;
        }

        if (iter >= PATH_HANDLE_DOTDOT_MAX_ITER)
        {
            errno = ELOOP;
            return ERR;
        }

        if (current->dentry != current->mount->root)
        {
            dentry_t* parent = current->dentry->parent;
            if (parent == NULL || parent == current->dentry)
            {
                errno = ENOENT;
                return ERR;
            }

            dentry_t* new_parent = REF(parent);
            DEREF(current->dentry);
            current->dentry = new_parent;
        }

        return 0;
    }
    else
    {
        assert(current->dentry->parent != NULL); // This can only happen if the filesystem is corrupt.
        dentry_t* parent = REF(current->dentry->parent);
        DEREF(current->dentry);
        current->dentry = parent;

        return 0;
    }
}

uint64_t path_walk_single_step(path_t* outPath, const path_t* parent, const char* component, walk_flags_t flags,
    namespace_t* ns)
{
    if (!vfs_is_name_valid(component))
    {
        errno = EINVAL;
        return ERR;
    }

    path_t current = PATH_EMPTY;
    path_copy(&current, parent);
    PATH_DEFER(&current);

    if (atomic_load(&current.dentry->mountCount) > 0)
    {
        path_t nextRoot = PATH_EMPTY;
        if (namespace_traverse_mount(ns, &current, &nextRoot) == ERR)
        {
            return ERR;
        }
        PATH_DEFER(&nextRoot);

        path_copy(&current, &nextRoot);
    }

    dentry_t* next = vfs_get_or_lookup_dentry(&current, component);
    if (next == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(next);

    if (atomic_load(&next->flags) & DENTRY_NEGATIVE)
    {
        if (flags & WALK_NEGATIVE_IS_OK)
        {
            path_set(outPath, current.mount, next);
            return 0;
        }
        errno = ENOENT;
        return ERR;
    }

    path_set(outPath, current.mount, next);
    return 0;
}

uint64_t path_walk(path_t* outPath, const pathname_t* pathname, const path_t* start, walk_flags_t flags,
    namespace_t* ns)
{
    if (!PATHNAME_IS_VALID(pathname))
    {
        errno = EINVAL;
        return ERR;
    }

    if (pathname->string[0] == '\0')
    {
        errno = EINVAL;
        return ERR;
    }

    path_t current = PATH_EMPTY;
    const char* p = pathname->string;
    if (pathname->string[0] == '/')
    {
        if (namespace_get_root_path(ns, &current) == ERR)
        {
            return ERR;
        }
        p++;
    }
    else
    {
        if (start == NULL || start->dentry == NULL || start->mount == NULL)
        {
            errno = EINVAL;
            return ERR;
        }
        path_copy(&current, start);
    }
    PATH_DEFER(&current);

    assert(current.dentry != NULL && current.mount != NULL);

    if (*p == '\0')
    {
        path_copy(outPath, &current);
        return 0;
    }

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

        if (*p == ':')
        {
            errno = EBADFLAG;
            return ERR;
        }

        const char* componentStart = p;
        while (*p != '\0' && *p != '/')
        {
            if (!PATH_VALID_CHAR(*p))
            {
                errno = EINVAL;
                return ERR;
            }
            p++;
        }

        uint64_t len = p - componentStart;
        if (len >= MAX_NAME)
        {
            errno = ENAMETOOLONG;
            return ERR;
        }

        memcpy(component, componentStart, len);
        component[len] = '\0';

        if (strcmp(component, ".") == 0)
        {
            continue;
        }

        if (strcmp(component, "..") == 0)
        {
            if (path_handle_dotdot(&current) == ERR)
            {
                return ERR;
            }
            continue;
        }

        path_t next = PATH_EMPTY;
        if (path_walk_single_step(&next, &current, component, flags, ns) == ERR)
        {
            return ERR;
        }

        path_copy(&current, &next);
        path_put(&next);

        if (atomic_load(&current.dentry->flags) & DENTRY_NEGATIVE)
        {
            if (flags & WALK_NEGATIVE_IS_OK)
            {
                break;
            }

            errno = ENOENT;
            return ERR;
        }
    }

    if (atomic_load(&current.dentry->mountCount) > 0)
    {
        path_t root = PATH_EMPTY;
        if (namespace_traverse_mount(ns, &current, &root) == ERR)
        {
            return ERR;
        }

        path_copy(&current, &root);
        path_put(&root);
    }

    path_copy(outPath, &current);
    return 0;
}

uint64_t path_walk_parent(path_t* outPath, const pathname_t* pathname, const path_t* start, char* outLastName,
    walk_flags_t flags, namespace_t* ns)
{
    if (!PATHNAME_IS_VALID(pathname) || outPath == NULL || outLastName == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (pathname->string[0] == '\0')
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
        if (start == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        strncpy(outLastName, string, MAX_NAME - 1);
        outLastName[MAX_NAME - 1] = '\0';

        path_copy(outPath, start);
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

    return path_walk(outPath, &parentPathname, start, flags, ns);
}

uint64_t path_to_name(const path_t* path, pathname_t* pathname)
{
    if (path == NULL || path->dentry == NULL || path->mount == NULL || pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(pathname->string, 0, MAX_PATH);
    pathname->flags = PATH_NONE;
    pathname->isValid = false;

    path_t current = PATH_EMPTY;
    path_copy(&current, path);
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
            path_set(&current, current.mount->parent, current.mount->mountpoint);
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
