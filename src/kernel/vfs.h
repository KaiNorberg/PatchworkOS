#pragma once

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

#include "defs.h"
#include "path.h"

#define FILE_DEFER(file) __attribute__((cleanup(file_defer_cleanup))) file_t* CONCAT(f, __COUNTER__) = (file)

typedef struct resource resource_t;

typedef struct fs fs_t;
typedef struct volume volume_t;
typedef struct file file_t;
typedef struct poll_file poll_file_t;

typedef uint64_t (*fs_mount_t)(const char*); // Add arguemnts as they are needed

typedef struct fs
{
    char* name;
    fs_mount_t mount;
} fs_t;

typedef uint64_t (*volume_unmount_t)(volume_t*);
typedef file_t* (*volume_open_t)(volume_t*, const path_t*);
typedef uint64_t (*volume_open2_t)(volume_t*, const path_t*, file_t* [2]);
typedef uint64_t (*volume_stat_t)(volume_t*, const path_t*, stat_t*);
typedef uint64_t (*volume_listdir_t)(volume_t*, const path_t*, dir_entry_t*, uint64_t);
typedef void (*volume_cleanup_t)(volume_t*, file_t*);

typedef struct volume_ops
{
    volume_unmount_t unmount;
    volume_open_t open;
    volume_open2_t open2;
    volume_stat_t stat;
    volume_listdir_t listdir;
    volume_cleanup_t cleanup;
} volume_ops_t;

typedef struct volume
{
    list_entry_t entry;
    char label[MAX_NAME];
    const volume_ops_t* ops;
    atomic_uint64_t ref;
} volume_t;

typedef struct wait_queue wait_queue_t;

typedef uint64_t (*file_read_t)(file_t*, void*, uint64_t);
typedef uint64_t (*file_write_t)(file_t*, const void*, uint64_t);
typedef uint64_t (*file_seek_t)(file_t*, int64_t, seek_origin_t);
typedef uint64_t (*file_ioctl_t)(file_t*, uint64_t, void*, uint64_t);
typedef uint64_t (*file_flush_t)(file_t*, const pixel_t*, uint64_t, const rect_t*);
typedef wait_queue_t* (*file_poll_t)(file_t*, poll_file_t*);

typedef struct file_ops
{
    file_read_t read;
    file_write_t write;
    file_seek_t seek;
    file_ioctl_t ioctl;
    file_flush_t flush;
    file_poll_t poll;
} file_ops_t;

typedef struct file
{
    volume_t* volume;
    uint64_t pos;
    void* private;
    resource_t* resource; // Used by sysfs
    const file_ops_t* ops;
    atomic_uint64_t ref;
} file_t;

typedef struct poll_file
{
    file_t* file;
    poll_event_t requested;
    poll_event_t occurred;
} poll_file_t;

file_t* file_new(volume_t* volume);

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

uint64_t vfs_chdir(const char* path);

file_t* vfs_open(const char* path);

uint64_t vfs_open2(const char* path, file_t* files[2]);

uint64_t vfs_stat(const char* path, stat_t* buffer);

uint64_t vfs_listdir(const char* path, dir_entry_t* entries, uint64_t amount);

uint64_t vfs_read(file_t* file, void* buffer, uint64_t count);

uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count);

uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin);

uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size);

uint64_t vfs_flush(file_t* file, const void* buffer, uint64_t size, const rect_t* rect);

uint64_t vfs_poll(poll_file_t* files, uint64_t amount, nsec_t timeout);

void dir_entry_push(dir_entry_t* entries, uint64_t amount, uint64_t* index, uint64_t* total, dir_entry_t* entry);