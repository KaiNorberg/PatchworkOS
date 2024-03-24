#include "utils.h"

#include <string.h>

#include "vfs/vfs.h"

bool vfs_valid_name(const char* name)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (VFS_END_OF_NAME(name[i]))
        {
            return true;
        }
        else if (!VFS_VALID_CHAR(name[i]))
        {
            return false;
        }
    }

    return false;
}

bool vfs_valid_path(const char* path)
{
    uint64_t i = 0;
    for (; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return true;
        }
        else if (path[i] == VFS_DISK_DELIMITER)
        {
            i++;
            break;
        }
        else if (!VFS_VALID_CHAR(path[i]))
        {
            return false;
        }
    }

    uint64_t nameLength = 0;
    for (; i < VFS_MAX_PATH_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return true;
        }
        else if (path[i] == VFS_NAME_DELIMITER)
        {
            nameLength = 0;
            continue;
        }
        else if (!VFS_VALID_CHAR(path[i]))
        {
            return false;
        }

        nameLength++;
        if (nameLength >= VFS_MAX_NAME_LENGTH)
        {
            return false;
        }
    }

    return false;
}

bool vfs_compare_names(const char* a, const char* b)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (VFS_END_OF_NAME(a[i]))
        {
            return VFS_END_OF_NAME(b[i]);
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

const char* vfs_next_dir(const char* path)
{
    const char* next = strchr(path, VFS_NAME_DELIMITER);
    if (next == NULL)
    {
        return NULL;
    }
    else
    {
        next += 1;
        if (strchr(next, VFS_NAME_DELIMITER) != NULL)
        {
            return next;
        }
        else
        {
            return NULL;
        }
    }
}

const char* vfs_basename(const char* path)
{
    const char* base = strrchr(path, VFS_NAME_DELIMITER);
    if (base == NULL)
    {
        return path;
    }
    else
    {
        return base + 1;
    }
}