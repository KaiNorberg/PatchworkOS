#pragma once

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/win.h>

#include "defs.h"
#include "list.h"
#include "lock.h"

#define VFS_NAME_SEPARATOR '/'
#define VFS_LABEL_SEPARATOR ':'

#define VFS_VALID_LETTER(ch) ((ch) >= VFS_LETTER_BASE && (ch) <= VFS_LETTER_MAX)
#define VFS_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define VFS_END_OF_NAME(ch) ((ch) == VFS_NAME_SEPARATOR || (ch) == '\0')
#define VFS_END_OF_LABEL(ch) ((ch) == VFS_LABEL_SEPARATOR || (ch) == '\0')

#define FILE_CALL(file, op, ...) ((file)->ops.op != NULL ? (file)->ops.op(file __VA_OPT__(, )##__VA_ARGS__) : ERROR(EACCES))

#define FILE_CALL_PTR(file, op, ...) ((file)->ops.op != NULL ? (file)->ops.op(file __VA_OPT__(, )##__VA_ARGS__) : NULLPTR(EACCES))

#define FILE_GUARD(file) __attribute__((cleanup(file_cleanup))) file_t* CONCAT(f, __COUNTER__) = (file)

typedef struct fs fs_t;
typedef struct volume volume_t;
typedef struct file file_t;

typedef struct fs
{
    char* name;
    uint64_t (*mount)(volume_t*);
} fs_t;

typedef struct volume
{
    list_entry_t base;
    char label[CONFIG_MAX_LABEL];
    fs_t* fs;
    uint64_t (*unmount)(volume_t*);
    uint64_t (*open)(volume_t*, file_t*, const char*);
    uint64_t (*stat)(volume_t*, const char*, stat_t*);
    _Atomic(uint64_t) ref;
} volume_t;

typedef struct file_ops
{
    uint64_t (*read)(file_t*, void*, uint64_t);
    uint64_t (*write)(file_t*, const void*, uint64_t);
    uint64_t (*seek)(file_t*, int64_t, uint8_t);
    uint64_t (*ioctl)(file_t*, uint64_t, void*, uint64_t);
    uint64_t (*flush)(file_t*, const void*, uint64_t, const rect_t* rect);
    void* (*mmap)(file_t*, void*, uint64_t, prot_t);
    bool (*write_avail)(file_t*);
    bool (*read_avail)(file_t*);
} file_ops_t;

typedef struct file
{
    volume_t* volume;
    uint64_t position;
    void* internal;
    void (*cleanup)(file_t*);
    file_ops_t ops;
    _Atomic(uint64_t) ref;
} file_t;

typedef struct
{
    file_t* file;
    uint16_t requested;
    uint16_t occurred;
} poll_file_t;

file_t* file_new(void);

file_t* file_ref(file_t* file);

void file_deref(file_t* file);

static inline void file_cleanup(file_t** file)
{
    file_deref(*file);
}

void vfs_init(void);

file_t* vfs_open(const char* path);

uint64_t vfs_stat(const char* path, stat_t* buffer);

uint64_t vfs_mount(const char* label, fs_t* fs);

uint64_t vfs_unmount(const char* label);

uint64_t vfs_realpath(char* out, const char* path);

uint64_t vfs_chdir(const char* path);

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, uint64_t timeout);

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
