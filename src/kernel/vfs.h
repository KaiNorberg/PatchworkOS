#pragma once

#include <stdatomic.h>
#include <string.h>
#include <ctype.h>
#include <sys/io.h>

#include "defs.h"
#include "lock.h"
#include "list.h"

#define VFS_NAME_SEPARATOR '/'
#define VFS_LABEL_SEPARATOR ':'

#define VFS_VALID_LETTER(ch) ((ch) >= VFS_LETTER_BASE && (ch) <= VFS_LETTER_MAX)
#define VFS_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define VFS_END_OF_NAME(ch) ((ch) == VFS_NAME_SEPARATOR || (ch) == '\0')
#define VFS_END_OF_LABEL(ch) ((ch) == VFS_LABEL_SEPARATOR || (ch) == '\0')

#define FILE_CALL_METHOD(file, method, ...) \
    ((file)->methods.method != NULL ? \
    (file)->methods.method(file, ##__VA_ARGS__) : \
    ERROR(EACCES))

#define FILE_CALL_METHOD_PTR(file, method, ...) \
    ((file)->methods.method != NULL ? \
    (file)->methods.method(file, ##__VA_ARGS__) : \
    NULLPTR(EACCES))

typedef struct Filesystem Filesystem;
typedef struct Volume Volume;
typedef struct File File;

typedef struct Filesystem
{
    char* name;
    uint64_t (*mount)(Volume*);
} Filesystem;

typedef struct Volume
{
    ListEntry base;
    char label[CONFIG_MAX_LABEL];
    Filesystem* fs;
    uint64_t (*unmount)(Volume*);
    uint64_t (*open)(Volume*, File*, const char*);
    uint64_t (*stat)(Volume*, const char*, stat_t*);
    _Atomic(uint64_t) ref;
} Volume;

typedef struct
{
    uint64_t (*read)(File*, void*, uint64_t);
    uint64_t (*write)(File*, const void*, uint64_t);
    uint64_t (*seek)(File*, int64_t, uint8_t);
    uint64_t (*ioctl)(File*, uint64_t, void*, uint64_t);
    void* (*mmap)(File*, void*, uint64_t, uint8_t);
    bool (*write_avail)(File*);
    bool (*read_avail)(File*);
} FileMethods;

typedef struct File
{
    Volume* volume;
    uint64_t position;
    void* internal;
    void (*cleanup)(File*);
    FileMethods methods;
    _Atomic(uint64_t) ref;
} File;

typedef struct
{
    File* file;
    uint16_t requested;
    uint16_t occurred;
} PollFile;

File* file_ref(File* file);

void file_deref(File* file);

void vfs_init(void);

File* vfs_open(const char* path);

uint64_t vfs_stat(const char* path, stat_t* buffer);

uint64_t vfs_mount(const char* label, Filesystem* fs);

uint64_t vfs_unmount(const char* label);

uint64_t vfs_realpath(char* out, const char* path);

uint64_t vfs_chdir(const char* path);

//Files should be null terminated.
uint64_t vfs_poll(PollFile* files, uint64_t timeout);

static inline uint64_t vfs_name_length(const char* name)
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

static inline void vfs_copy_name(char* dest, const char* src)
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

static inline bool vfs_compare_labels(const char* a, const char* b)
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

static inline bool vfs_compare_names(const char* a, const char* b)
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