#pragma once

#include <ctype.h>
#include <string.h>

#include "defs/defs.h"
#include "vfs/vfs.h"

#define VFS_NAME_SEPARATOR '/'
#define VFS_DRIVE_SEPARATOR ':'

#define VFS_LETTER_BASE 'A'
#define VFS_LETTER_AMOUNT ('Z' - 'A' + 1)

#define VFS_VALID_CHAR(ch) (isalnum(ch) || (ch) == '_' || (ch) == '.')
#define VFS_VALID_LETTER(letter) ((letter) >= 'A' && (letter) <= 'Z')

#define VFS_END_OF_NAME(ch) ((ch) == VFS_NAME_SEPARATOR || (ch) == '\0')

static inline void vfs_copy_name(char* dest, const char* src)
{
    for (uint64_t i = 0; i < CONFIG_MAX_PATH - 1; i++)
    {
        if (VFS_END_OF_NAME(src[i]))
        {
            dest[i] = '\0';
            return;
        }
        else
        {
            dest[i] = src[i];
        }
    }
    dest[CONFIG_MAX_PATH - 1] = '\0';
}

static inline bool vfs_compare_names(const char* a, const char* b)
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

static inline const char* vfs_first_name(const char* path)
{
    if (path[0] == VFS_NAME_SEPARATOR)
    {
        return path + 1;
    }
    
    return path;
}

static inline const char* vfs_first_dir(const char* path)
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

static inline const char* vfs_next_dir(const char* path)
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

static inline const char* vfs_next_name(const char* path)
{
    const char* base = strchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : NULL;
}

static inline const char* vfs_basename(const char* path)
{
    const char* base = strrchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : path;
}

static inline uint64_t vfs_parent_dir(char* dest, const char* src)
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