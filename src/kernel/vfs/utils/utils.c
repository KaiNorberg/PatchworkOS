#include "utils.h"

#include "vfs/vfs.h"

#include "tty/tty.h"

#include <libc/string.h>
#include <libc/ctype.h>

uint8_t vfs_utils_validate_char(char ch)
{
    return isalnum(ch) || ch == '_' || ch == '.';
}

uint8_t vfs_utils_validate_name(const char* name)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (name[i] == '\0')
        {
            return 1;
        }
        else if (!vfs_utils_validate_char(name[i]))
        {
            return 0;
        }   
    }

    return 0;
}

uint8_t vfs_utils_validate_path(const char* path)
{
    uint64_t i = 0;
    for (; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return 0;
        }
        else if (path[i] == VFS_DISK_DELIMITER)
        {
            i++;
            break;
        }
        else if (!vfs_utils_validate_char(path[i]))
        {
            return 0;
        }   
    }

    uint64_t nameLength = 0;
    for (; i < VFS_MAX_PATH_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return 1;
        }
        else if (path[i] == VFS_NAME_DELIMITER)
        {
            nameLength = 0;
            continue;
        }
        else if (!vfs_utils_validate_char(path[i]))
        {
            return 0;
        }

        nameLength++;
        if (nameLength >= VFS_MAX_NAME_LENGTH)
        {
            return 0;
        }
    }

    return 0;
}

const char* vfs_utils_first_dir(const char* path)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return 0;
        }
        else if (path[i] == VFS_NAME_DELIMITER)
        {
            return path;
        }
    }

    return 0;
}

const char* vfs_utils_next_dir(const char* path)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (path[i] == '\0')
        {
            return 0;
        }
        else if (path[i] == VFS_NAME_DELIMITER)
        {
            for (uint64_t j = 1; j < VFS_MAX_NAME_LENGTH; j++)
            {
                if (path[i + j] == '\0')
                {
                    return 0;
                }
                else if (path[i + j] == VFS_NAME_DELIMITER)
                {
                    return (const char*)((uint64_t)path + i + 1);
                }
            }
        }
    }

    return 0;
}

const char* vfs_utils_basename(const char* path)
{
    uint64_t length = strlen(path);

    if (length == 0)
    {
        return 0;
    }

    for (int64_t i = length - 1; i >= 0; i--)
    {
        if (path[i] == VFS_NAME_DELIMITER)
        {
            if (path[i + 1] == '\0')
            {
                return 0;
            }
            else
            {
                return (const char*)((uint64_t)path + i + 1);
            }
        }
    }

    return 0;
}

uint8_t vfs_utils_compare_names(const char* name1, const char* name2)
{
    while (1)
    {
        if (*name1 == '\0' || *name1 == VFS_NAME_DELIMITER)
        {
            return *name2 == '\0' || *name2 == VFS_NAME_DELIMITER;
        }
        else if (*name1 != *name2)
        {
            return 0;
        }

        name1++;
        name2++;
    }
}