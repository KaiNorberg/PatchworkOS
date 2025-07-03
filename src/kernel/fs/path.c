#include "path.h"

#include "log/log.h"

#include "vfs.h"

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

uint64_t path_parse_pathname(parsed_pathname_t* dest, const char* pathname)
{
    if (dest == NULL || pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(dest->pathname, 0, MAX_NAME);
    dest->flags = PATH_NONE;

    uint64_t length = strnlen_s(pathname, MAX_PATH);
    if (length >= MAX_PATH)
    {
        errno = ENAMETOOLONG;
        return ERR;
    }

    uint64_t index = 0;
    uint64_t currentNameLength = 0;
    while (pathname[index] != '\0' && pathname[index] != '?')
    {
        if (pathname[index] == '/')
        {
            currentNameLength = 0;
        }
        else
        {
            if (!PATH_VALID_CHAR(pathname[index]))
            {
                errno = EBADPATH;
                return ERR;
            }
            currentNameLength++;
            if (currentNameLength >= MAX_NAME)
            {
                errno = ENAMETOOLONG;
                return ERR;
            }
        }

        dest->pathname[index] = pathname[index];
        index++;
    }

    dest->pathname[index] = '\0';

    if (pathname[index] != '?')
    {
        return 0;
    }

    index++; // Skip '?'.
    const char* flags = &pathname[index];

    while (true)
    {
        while (pathname[index] == '&')
        {
            index++;
        }

        if (pathname[index] == '\0')
        {
            break;
        }

        const char* token = &pathname[index];
        while (pathname[index] != '\0' && pathname[index] != '&')
        {
            if (!isalnum(pathname[index]))
            {
                errno = EBADFLAG;
                return ERR;
            }
            index++;
        }

        uint64_t tokenLength = &pathname[index] - token;
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

        dest->flags |= flag->flag;
    }

    return 0;
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
            if (parent == NULL)
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

static uint64_t path_traverse_component(path_t* current, const char* component)
{
    lock_acquire(&current->dentry->lock);
    if (current->dentry->flags & DENTRY_MOUNTPOINT)
    {
        path_t nextRoot;
        if (vfs_mountpoint_to_mount_root(&nextRoot, current) == ERR)
        {
            lock_release(&current->dentry->lock);
            return ERR;
        }

        path_put(current);
        path_copy(current, &nextRoot);
        path_put(&nextRoot);
        return 0;
    }
    lock_release(&current->dentry->lock);

    dentry_t* next = vfs_get_or_lookup_dentry(current->dentry, component);
    if (next == NULL)
    {
        return ERR;
    }

    dentry_deref(current->dentry);
    current->dentry = next;

    return 0;
}

uint64_t path_walk(path_t* outPath, const char* pathname, const path_t* start)
{
    if (pathname == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (pathname[0] == '\0')
    {
        errno = EINVAL;
        return ERR;
    }

    path_t current = {0};
    const char* p = pathname;

    if (pathname[0] == '/')
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
        current.dentry = dentry_ref(start->dentry);
        current.mount = mount_ref(start->mount);
    }

    if (*p == '\0')
    {
        path_copy(outPath, &current);
        path_put(&current);
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
            path_put(&current);
            errno = EBADFLAG;
            return ERR;
        }

        const char* componentStart = p;
        while (*p != '\0' && *p != '/')
        {
            if (!PATH_VALID_CHAR(*p))
            {
                path_put(&current);
                errno = EINVAL;
                return ERR;
            }
            p++;
        }

        uint64_t len = p - componentStart;
        if (len >= MAX_NAME)
        {
            path_put(&current);
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
                path_put(&current);
                return ERR;
            }
            continue;
        }

        if (path_traverse_component(&current, component) == ERR)
        {
            path_put(&current);
            return ERR;
        }
    }

    path_copy(outPath, &current);
    path_put(&current);
    return 0;
}

uint64_t path_walk_parent(path_t* outPath, const char* pathname, const path_t* start, char* outLastName)
{
    if (pathname == NULL || outLastName == NULL)
    {
        return ERR;
    }

    memset(outLastName, 0, MAX_NAME);

    const char* lastSlash = strrchr(pathname, '/');
    if (lastSlash == NULL)
    {
        strcpy(outLastName, pathname);
        path_copy(outPath, start);
        return 0;
    }

    strcpy(outLastName, lastSlash + 1);

    if (lastSlash == pathname)
    {
        vfs_get_global_root(outPath);
        return 0;
    }

    uint64_t parentLen = lastSlash - pathname;
    char parentPath[MAX_PATH];

    memcpy(parentPath, pathname, parentLen);
    parentPath[parentLen] = '\0';

    if (!vfs_is_name_valid(outLastName))
    {
        errno = EINVAL;
        return NULL;
    }

    return path_walk(outPath, parentPath, start);
}

void path_copy(path_t* dest, const path_t* src)
{
    dest->dentry = NULL;
    dest->mount = NULL;

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