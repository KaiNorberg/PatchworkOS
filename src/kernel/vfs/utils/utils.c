#include "utils.h"

#include <string.h>

#include "vfs/vfs.h"
#include "heap/heap.h"

File* file_new(Drive* drive, void* context)
{
    File* file = kmalloc(sizeof(File));
    file->internal = context;
    file->drive = drive;
    atomic_init(&file->position, 0);
    atomic_init(&file->ref, 1);

    return file;
}

bool vfs_compare_names(const char* a, const char* b)
{
    for (uint64_t i = 0; i < CONFIG_MAX_PATH; i++)
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

const char* vfs_first_dir(const char* path)
{
    if (path[0] == VFS_NAME_SEPARATOR)
    {
        path++;
    }

    if (strchr(path, VFS_NAME_SEPARATOR) == NULL)
    {
        return NULL;
    }
    else
    {
        return path;
    }
}

const char* vfs_next_dir(const char* path)
{
    const char* next = strchr(path, VFS_NAME_SEPARATOR);
    if (next == NULL)
    {
        return NULL;
    }
    else
    {
        next += 1;
        if (strchr(next, VFS_NAME_SEPARATOR) != NULL)
        {
            return next;
        }
        else
        {
            return NULL;
        }
    }
}

const char* vfs_next_name(const char* path)
{
    const char* base = strchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : NULL;
}

const char* vfs_basename(const char* path)
{
    const char* base = strrchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : path;
}

uint64_t vfs_parent_dir(char* dest, const char* src)
{
    char* end = strrchr(src, VFS_NAME_SEPARATOR);
    if (end == NULL)
    {
        return ERR;
    }

    strncpy(dest, src, end - src);
    dest[end - src] = '\0';

    return 0;
}