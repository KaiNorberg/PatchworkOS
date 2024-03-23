#include "utils.h"

#include "vfs/vfs.h"

uint8_t vfs_valid_name(const char* name)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (VFS_END_OF_NAME(name[i]))
        {
            return 1;
        }
        else if (!VFS_VALID_CHAR(name[i]))
        {
            return 0;
        }
    }

    return 0;
}

uint8_t vfs_valid_path(const char* path)
{
    return 0;
}

uint8_t vfs_compare_names(const char* a, const char* b)
{
    for (uint64_t i = 0; i < VFS_MAX_NAME_LENGTH; i++)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
        else if (VFS_END_OF_NAME(a[i]))
        {
            return 1;
        }
    }

    return 0;
}