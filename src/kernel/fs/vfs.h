#pragma once

#include "defs.h"
#include "path.h"
#include "sysfs.h"

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

#define FILE_DEFER(file) __attribute__((cleanup(file_defer_cleanup))) file_t* CONCAT(f, __COUNTER__) = (file)

typedef struct fs fs_t;
typedef struct volume volume_t;
typedef struct file file_t;
typedef struct poll_file poll_file_t;

typedef uint64_t (*fs_mount_t)(const char*); // Add arguments as they are needed

typedef struct fs
{
    char* name;
    fs_mount_t mount;
} fs_t;

typedef uint64_t (*volume_unmount_t)(volume_t*);
typedef file_t* (*volume_open_t)(volume_t*, const path_t*);
typedef uint64_t (*volume_open2_t)(volume_t*, const path_t*, file_t* [2]);
typedef uint64_t (*volume_stat_t)(volume_t*, const path_t*, stat_t*);
typedef uint64_t (*volume_rename_t)(volume_t*, const path_t*, const path_t*);
typedef uint64_t (*volume_remove_t)(volume_t*, const path_t*);
typedef void (*volume_cleanup_t)(volume_t*, file_t*);

typedef struct volume_ops
{
    volume_unmount_t unmount;
    volume_open_t open;
    volume_open2_t open2;
    volume_stat_t stat;
    volume_rename_t rename;
    volume_remove_t remove;
    volume_cleanup_t cleanup;
} volume_ops_t;

typedef struct volume
{
    list_entry_t entry;
    char label[MAX_NAME];
    const volume_ops_t* ops;
    sysdir_t sysdir;
    atomic_uint64 ref;
} volume_t;

typedef struct wait_queue wait_queue_t;

typedef uint64_t (*file_read_t)(file_t*, void*, uint64_t);
typedef uint64_t (*file_write_t)(file_t*, const void*, uint64_t);
typedef uint64_t (*file_seek_t)(file_t*, int64_t, seek_origin_t);
typedef uint64_t (*file_ioctl_t)(file_t*, uint64_t, void*, uint64_t);
typedef wait_queue_t* (*file_poll_t)(file_t*, poll_file_t*);
typedef void* (*file_mmap_t)(file_t*, void*, uint64_t, prot_t);
typedef uint64_t (*file_readdir_t)(file_t*, stat_t*, uint64_t);

typedef struct file_ops
{
    file_read_t read;
    file_write_t write;
    file_seek_t seek;
    file_ioctl_t ioctl;
    file_poll_t poll;
    file_mmap_t mmap;
    file_readdir_t readdir;
} file_ops_t;

typedef struct file
{
    volume_t* volume;
    uint64_t pos;
    void* private;
    syshdr_t* syshdr; // Used by sysfs
    const file_ops_t* ops;
    path_flags_t flags;
    atomic_uint64 ref;
    path_t path;
} file_t;

typedef struct poll_file
{
    file_t* file;
    poll_event_t events;
    poll_event_t revents;
} poll_file_t;

file_t* file_new(volume_t* volume, const path_t* path, path_flags_t supportedFlags);

file_t* file_ref(file_t* file);

void file_deref(file_t* file);

static inline void file_defer_cleanup(file_t** file)
{
    file_deref(*file);
}

void vfs_init(void);

uint64_t vfs_attach_simple(const char* label, const volume_ops_t* ops);

uint64_t vfs_mount(const char* label, fs_t* fs);

uint64_t vfs_unmount(const char* label);

uint64_t vfs_chdir(const path_t* path);

file_t* vfs_open(const path_t* path);

uint64_t vfs_open2(const path_t* path, file_t* files[2]);

uint64_t vfs_stat(const path_t* path, stat_t* buffer);

uint64_t vfs_rename(const path_t* oldpath, const path_t* newpath);

uint64_t vfs_remove(const path_t* path);

uint64_t vfs_readdir(file_t* file, stat_t* infos, uint64_t amount);

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count);

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count);

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin);

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size);

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout);

void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot);

// Helper function for implementing readdir
void readdir_push(stat_t* infos, uint64_t amount, uint64_t* index, uint64_t* total, stat_t* info);

// Helper macros for implementing file operations dealing with simple buffers
#define BUFFER_READ(file, buffer, count, src, size) \
    ({ \
        uint64_t readCount = (file->pos <= (size)) ? MIN((count), (size) - file->pos) : 0; \
        memcpy((buffer), (src) + file->pos, readCount); \
        file->pos += readCount; \
        readCount; \
    })

#define BUFFER_SEEK(file, offset, origin, size) \
    ({ \
        uint64_t position; \
        switch (origin) \
        { \
        case SEEK_SET: \
        { \
            position = offset; \
        } \
        break; \
        case SEEK_CUR: \
        { \
            position = file->pos + (offset); \
        } \
        break; \
        case SEEK_END: \
        { \
            position = (size) - (offset); \
        } \
        break; \
        default: \
        { \
            position = 0; \
        } \
        break; \
        } \
        file->pos = MIN(position, (size)); \
        file->pos; \
    })
