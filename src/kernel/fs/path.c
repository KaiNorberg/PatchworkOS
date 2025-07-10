#include "path.h"

#include "fs/dentry.h"
#include "log/log.h"

#include "vfs.h"

#include <_internal/MAX_NAME.h>
#include <_internal/MAX_PATH.h>
#include <errno.h>
#include <string.h>

static map_t flagMap;
static path_flag_entry_t flagEntries[] = {
    {.flag = PATH_NONBLOCK, .name = "nonblock"},
    {.flag = PATH_APPEND, .name = "append"},
    {.flag = PATH_CREATE, .name = "create"},
    {.flag = PATH_EXCLUSIVE, .name = "exclusive"},
    {.flag = PATH_TRUNCATE, .name = "trunc"},
    {.flag = PATH_DIRECTORY, .name = "dir"},
};

void path_flags_init(void)
{
    map_init(&flagMap);

    for (uint64_t i = 0; i < sizeof(flagEntries) / sizeof(flagEntries[0]); i++)
    {
        map_entry_init(&flagEntries[i].entry);
        map_key_t key = map_key_string(flagEntries[i].name);
        assert(map_insert(&flagMap, &key, &flagEntries[i].entry) != ERR);
    }
}

uint64_t pathname_init(pathname_t* pathname, const char* string)
{
    if (pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(pathname->string, 0, MAX_NAME);
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
    while (string[index] != '\0' && string[index] != '?')
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

    if (string[index] != '?')
    {
        pathname->isValid = true;
        return 0;
    }

    index++; // Skip '?'.
    const char* flags = &string[index];

    while (true)
    {
        while (string[index] == '&')
        {
            index++;
        }

        if (string[index] == '\0')
        {
            break;
        }

        const char* token = &string[index];
        while (string[index] != '\0' && string[index] != '&')
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

        map_key_t key = map_key_buffer(token, tokenLength);
        path_flag_entry_t* flag = CONTAINER_OF_SAFE(map_get(&flagMap, &key), path_flag_entry_t, entry);
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
        dentry_deref(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        mount_deref(path->mount);
        path->mount = NULL;
    }

    if (dentry != NULL)
    {
        path->dentry = dentry_ref(dentry);
    }

    if (mount != NULL)
    {
        path->mount = mount_ref(mount);
    }
}

void path_copy(path_t* dest, const path_t* src)
{
    if (dest->dentry != NULL)
    {
        dentry_deref(dest->dentry);
        dest->dentry = NULL;
    }

    if (dest->mount != NULL)
    {
        mount_deref(dest->mount);
        dest->mount = NULL;
    }

    if (src->dentry != NULL)
    {
        dest->dentry = dentry_ref(src->dentry);
    }

    if (src->mount != NULL)
    {
        dest->mount = mount_ref(src->mount);
    }
}

void path_put(path_t* path)
{
    if (path->dentry != NULL)
    {
        dentry_deref(path->dentry);
        path->dentry = NULL;
    }

    if (path->mount != NULL)
    {
        mount_deref(path->mount);
        path->mount = NULL;
    }
}

static uint64_t path_handle_dotdot(path_t* current)
{
    if (current->dentry == current->mount->superblock->root)
    {
        uint64_t iter = 0;

        while (current->dentry == current->mount->superblock->root && iter < PATH_HANDLE_DOTDOT_MAX_ITER)
        {
            if (current->mount->parent == NULL || current->mount->mountpoint == NULL)
            {
                return 0;
            }

            mount_t* newMount = mount_ref(current->mount->parent);
            dentry_t* newDentry = dentry_ref(current->mount->mountpoint);

            mount_deref(current->mount);
            current->mount = newMount;
            dentry_deref(current->dentry);
            current->dentry = newDentry;

            iter++;
        }

        if (iter >= PATH_HANDLE_DOTDOT_MAX_ITER)
        {
            errno = ELOOP;
            return ERR;
        }

        if (current->dentry != current->mount->superblock->root)
        {
            dentry_t* parent = current->dentry->parent;
            if (parent == NULL || parent == current->dentry)
            {
                errno = ENOENT;
                return ERR;
            }

            dentry_t* new_parent = dentry_ref(parent);
            dentry_deref(current->dentry);
            current->dentry = new_parent;
        }

        return 0;
    }
    else
    {
        assert(current->dentry->parent != NULL); // This can only happen if the filesystem is corrupt.
        dentry_t* parent = dentry_ref(current->dentry->parent);
        dentry_deref(current->dentry);
        current->dentry = parent;

        return 0;
    }
}

uint64_t path_traverse(path_t* outPath, const path_t* parent, const char* component)
{
    if (!vfs_is_name_valid(component))
    {
        errno = EINVAL;
        return ERR;
    }

    path_t current = PATH_EMPTY;
    path_copy(&current, parent);
    PATH_DEFER(&current);

    lock_acquire(&current.dentry->lock);
    if (current.dentry->flags & DENTRY_MOUNTPOINT)
    {
        path_t nextRoot = PATH_EMPTY;
        if (vfs_mountpoint_to_mount_root(&nextRoot, &current) == ERR)
        {
            lock_release(&current.dentry->lock);
            return ERR;
        }
        PATH_DEFER(&nextRoot);

        lock_release(&current.dentry->lock);
        path_copy(&current, &nextRoot);
    }
    else
    {
        lock_release(&current.dentry->lock);
    }

    dentry_t* next = vfs_get_or_lookup_dentry(&current, component);
    if (next == NULL)
    {
        return ERR;
    }

    lock_acquire(&next->lock);
    if (next->flags & DENTRY_NEGATIVE)
    {
        dentry_deref(next);
        lock_release(&next->lock);
        errno = ENOENT;
        return ERR;
    }
    lock_release(&next->lock);

    path_set(outPath, current.mount, next);
    return 0;
}

uint64_t path_walk(path_t* outPath, const pathname_t* pathname, const path_t* start)
{
    if (pathname == NULL || !pathname->isValid)
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
        if (vfs_get_global_root(&current) == ERR)
        {
            return ERR;
        }
        p++;
    }
    else
    {
        if (start == NULL)
        {
            errno = EINVAL;
            return ERR;
        }
        path_copy(&current, start);
    }

    PATH_DEFER(&current);

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

        if (*p == '?')
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
        if (path_traverse(&next, &current, component) == ERR)
        {
            return ERR;
        }
        PATH_DEFER(&next);

        path_copy(&current, &next);
    }

    path_copy(outPath, &current);
    return 0;
}

uint64_t path_walk_parent(path_t* outPath, const pathname_t* pathname, const path_t* start, char* outLastName)
{
    if (pathname == NULL || !pathname->isValid || outLastName == NULL)
    {
        return ERR;
    }

    memset(outLastName, 0, MAX_NAME);

    const char* lastSlash = strrchr(pathname->string, '/');
    if (lastSlash == NULL)
    {
        strcpy(outLastName, pathname->string);
        path_copy(outPath, start);
        return 0;
    }

    strcpy(outLastName, lastSlash + 1);

    if (lastSlash == pathname->string)
    {
        vfs_get_global_root(outPath);
        return 0;
    }

    uint64_t parentLen = lastSlash - pathname->string;
    pathname_t parentPath = {0};

    memcpy(parentPath.string, pathname, parentLen);
    parentPath.string[parentLen] = '\0';
    parentPath.flags = PATH_NONE;

    if (!vfs_is_name_valid(outLastName))
    {
        errno = EINVAL;
        return ERR;
    }

    return path_walk(outPath, &parentPath, start);
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

    memmove(pathname->string, pathname->string + index, MAX_PATH - index);
    pathname->isValid = true;
    return 0;
}
