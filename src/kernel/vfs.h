#pragma once

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

#include "defs.h"
#include "sched.h"

#define VFS_NAME_SEPARATOR '/'
#define VFS_LABEL_SEPARATOR ':'

#define VFS_VALID_LETTER(ch) ((ch) >= VFS_LETTER_BASE && (ch) <= VFS_LETTER_MAX)
#define VFS_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define VFS_END_OF_NAME(ch) ((ch) == VFS_NAME_SEPARATOR || (ch) == '\0')
#define VFS_END_OF_LABEL(ch) ((ch) == VFS_LABEL_SEPARATOR || (ch) == '\0')

#define FILE_GUARD(file) __attribute__((cleanup(file_cleanup))) file_t* CONCAT(f, __COUNTER__) = (file)

typedef struct resource resource_t;

typedef struct fs fs_t;
typedef struct volume volume_t;
typedef struct file file_t;
typedef struct poll_file poll_file_t;

typedef uint64_t (*fs_mount_t)(const char*); // Add arguemnts as they are needed

typedef uint64_t (*volume_unmount_t)(volume_t*);
typedef uint64_t (*volume_open_t)(volume_t*, file_t*, const char*);
typedef uint64_t (*volume_stat_t)(volume_t*, const char*, stat_t*);

typedef uint64_t (*file_open_t)(file_t*, const char*);
typedef void (*file_cleanup_t)(file_t*);
typedef uint64_t (*file_read_t)(file_t*, void*, uint64_t);
typedef uint64_t (*file_write_t)(file_t*, const void*, uint64_t);
typedef uint64_t (*file_seek_t)(file_t*, int64_t, uint8_t);
typedef uint64_t (*file_ioctl_t)(file_t*, uint64_t, void*, uint64_t);
typedef uint64_t (*file_flush_t)(file_t*, const void*, uint64_t, const rect_t*);
typedef void* (*file_mmap_t)(file_t*, void*, uint64_t, prot_t);
typedef uint64_t (*file_status_t)(file_t*, poll_file_t*);

typedef struct file_ops
{
    file_open_t open;
    file_cleanup_t cleanup;
    file_read_t read;
    file_write_t write;
    file_seek_t seek;
    file_ioctl_t ioctl;
    file_flush_t flush;
    file_mmap_t mmap;
    file_status_t status;
} file_ops_t;

typedef struct volume_ops
{
    volume_unmount_t unmount;
    volume_stat_t stat;
} volume_ops_t;

typedef struct fs
{
    char* name;
    fs_mount_t mount;
} fs_t;

typedef struct volume
{
    list_entry_t base;
    char label[CONFIG_MAX_LABEL];
    const volume_ops_t* volumeOps;
    const file_ops_t* fileOps;
    atomic_uint64_t ref;
} volume_t;

typedef struct file
{
    volume_t* volume;
    uint64_t position;
    void* private;
    resource_t* resource; // Used by sysfs
    const file_ops_t* ops;
    atomic_uint64_t ref;
} file_t;

typedef struct poll_file
{
    file_t* file;
    uint16_t requested;
    uint16_t occurred;
} poll_file_t;

file_t* file_new(const file_ops_t* ops);

file_t* file_ref(file_t* file);

void file_deref(file_t* file);

static inline void file_cleanup(file_t** file)
{
    file_deref(*file);
}

void vfs_init(void);

uint64_t vfs_attach_simple(const char* label, const volume_ops_t* volumeOps, const file_ops_t* fileOps);

uint64_t vfs_mount(const char* label, fs_t* fs);

uint64_t vfs_unmount(const char* label);

file_t* vfs_open(const char* path);

uint64_t vfs_stat(const char* path, stat_t* buffer);

uint64_t vfs_realpath(char* out, const char* path);

uint64_t vfs_chdir(const char* path);

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, uint64_t timeout);

static inline uint64_t vfs_read(file_t* file, void* buffer, uint64_t count)
{
    if (file->ops->read == NULL)
    {
        return ERROR(EACCES);
    }
    return file->ops->read(file, buffer, count);
}

static inline uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count)
{
    if (file->ops->write == NULL)
    {
        return ERROR(EACCES);
    }
    return file->ops->write(file, buffer, count);
}

static inline uint64_t vfs_seek(file_t* file, int64_t offset, uint8_t origin)
{
    if (file->ops->seek == NULL)
    {
        return ERROR(EACCES);
    }
    return file->ops->seek(file, offset, origin);
}

static inline uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size)
{
    if (file->ops->ioctl == NULL)
    {
        return ERROR(EACCES);
    }
    return file->ops->ioctl(file, request, argp, size);
}

static inline uint64_t vfs_flush(file_t* file, const void* buffer, uint64_t count, const rect_t* rect)
{
    if (file->ops->flush == NULL)
    {
        return ERROR(EACCES);
    }
    return file->ops->flush(file, buffer, count, rect);
}

static inline void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot)
{
    if (file->ops->mmap == NULL)
    {
        return NULLPTR(EACCES);
    }
    return file->ops->mmap(file, address, length, prot);
}

static inline const char* vfs_basename(const char* path)
{
    const char* base = strrchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : path;
}

static inline uint64_t vfs_parent_dir(char* dest, const char* src)
{
    const char* end = strrchr(src, VFS_NAME_SEPARATOR);
    if (end == NULL)
    {
        return ERR;
    }

    strncpy(dest, src, end - src);
    dest[end - src] = '\0';

    return 0;
}

static inline const char* name_first(const char* path)
{
    if (path[0] == '\0')
    {
        return NULL;
    }
    else if (path[0] == VFS_NAME_SEPARATOR)
    {
        return path + 1;
    }

    return path;
}

static inline const char* name_next(const char* path)
{
    const char* base = strchr(path, VFS_NAME_SEPARATOR);
    return base != NULL ? base + 1 : NULL;
}

static inline uint64_t name_length(const char* name)
{
    for (uint64_t i = 0; i < MAX_PATH - 1; i++)
    {
        if (VFS_END_OF_NAME(name[i]))
        {
            return i;
        }
    }

    return MAX_PATH - 1;
}

static inline void name_copy(char* dest, const char* src)
{
    for (uint64_t i = 0; i < CONFIG_MAX_NAME - 1; i++)
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
    dest[CONFIG_MAX_NAME - 1] = '\0';
}

static inline bool name_compare(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
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

static inline bool name_valid(const char* name)
{
    uint64_t length = name_length(name);
    for (uint64_t i = 0; i < length; i++)
    {
        if (!VFS_VALID_CHAR(name[i]))
        {
            return false;
        }
    }

    return true;
}

static inline bool name_is_last(const char* name)
{
    return strchr(name, VFS_NAME_SEPARATOR) == NULL;
}

static inline bool label_compare(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
    {
        if (VFS_END_OF_LABEL(a[i]))
        {
            return VFS_END_OF_LABEL(b[i]);
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

static inline const char* dir_name_first(const char* path)
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

static inline const char* dir_name_next(const char* path)
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
