#pragma once

#include "path.h"

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>

typedef struct wait_queue wait_queue_t;

typedef struct file file_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;
typedef struct inode inode_t;
typedef struct poll_file poll_file_t;

#define FILE_DEFER(file) __attribute__((cleanup(file_defer_cleanup))) file_t* CONCAT(i, __COUNTER__) = (file)

typedef struct file
{
    atomic_uint64_t ref;
    uint64_t pos;
    path_flags_t flags;
    dentry_t* dentry;
    const file_ops_t* ops;
    void* private;
} file_t;

typedef struct file_ops
{
    uint64_t (*open)(file_t* file);
    uint64_t (*open2)(file_t* files[2]);
    void (*cleanup)(file_t* file);
    uint64_t (*read)(file_t* file, void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*write)(file_t* file, const void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*seek)(file_t* file, int64_t offset, seek_origin_t origin);
    uint64_t (*ioctl)(file_t* file, uint64_t request, void* argp, uint64_t size);
    wait_queue_t* (*poll)(file_t* file, poll_file_t* pollFile); // TODO: Overhaul polling.
    void* (*mmap)(file_t* file, void* address, uint64_t length, prot_t prot);
    uint64_t (*getdirent)(file_t* file, dirent_t* buffer, uint64_t amount);
} file_ops_t;

typedef struct poll_file
{
    file_t* file;
    poll_events_t events;
    poll_events_t occoured;
} poll_file_t;

file_t* file_new(dentry_t* dentry, path_flags_t flags);

void file_free(file_t* file);

file_t* file_ref(file_t* file);

void file_deref(file_t* file);

/**
 * @brief Helper function for basic seeking.
 * @ingroup kernel_vfs
 *
 */
uint64_t file_generic_seek(file_t* file, int64_t offset, seek_origin_t origin);

static inline void file_defer_cleanup(file_t** file)
{
    if (*file != NULL)
    {
        file_deref(*file);
    }
}