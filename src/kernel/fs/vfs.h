#pragma once

#include "defs.h"
#include "sync/rwlock.h"
#include "sysfs.h"
#include "utils/map.h"
#include "sync/lock.h"

#include <ctype.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef struct wait_queue wait_queue_t;

// TODO: Implement improved caching, LRU. Let the map_t handle static buffer + wrapper?
// TODO: Implement per-process namespaces.
// TODO: Implement literally everything else.

#define VFS_HANDLE_DOTDOT_MAX_ITER 1000

#define VFS_ROOT_ENTRY_NAME "__root__"

#define VFS_DEVICE_NAME_NONE "__none__"

#define VFS_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define SUPER_DEFER(superblock) \
    __attribute__((cleanup(superblock_defer_cleanup))) superblock_t* CONCAT(i, __COUNTER__) = (superblock)
#define INODE_DEFER(inode) __attribute__((cleanup(inode_defer_cleanup))) inode_t* CONCAT(i, __COUNTER__) = (inode)
#define DENTRY_DEFER(entry) __attribute__((cleanup(dentry_defer_cleanup))) dentry_t* CONCAT(i, __COUNTER__) = (entry)
#define FILE_DEFER(file) __attribute__((cleanup(file_defer_cleanup))) file_t* CONCAT(i, __COUNTER__) = (file)
#define MOUNT_DEFER(mount) __attribute__((cleanup(mount_defer_cleanup))) mount_t* CONCAT(i, __COUNTER__) = (mount)
#define PATH_DEFER(path) __attribute__((cleanup(path_defer_cleanup))) path_t* CONCAT(i, __COUNTER__) = (path)

typedef struct filesystem filesystem_t;
typedef struct superblock superblock_t;
typedef struct inode inode_t;
typedef struct dentry dentry_t;
typedef struct file file_t;
typedef struct mount mount_t;
typedef struct poll_file poll_file_t;

typedef struct inode_ops inode_ops_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct superblock_ops super_ops_t;
typedef struct file_ops file_ops_t;

typedef uint64_t superblock_id_t;
typedef uint64_t inode_id_t;
typedef uint64_t dentry_id_t;
typedef uint64_t mount_id_t;

typedef enum
{
    INODE_FILE,
    INODE_DIR,
    INODE_OBJ,
} inode_type_t;

typedef enum
{
    DENTRY_NONE = 0,
    DENTRY_MOUNTPOINT = 1 << 0,
} dentry_flags_t;

typedef enum
{
    PATH_NONE = 0,
    PATH_NONBLOCK = 1 << 0,
    PATH_APPEND = 1 << 1,
    PATH_CREATE = 1 << 2,
    PATH_EXCLUSIVE = 1 << 3,
    PATH_TRUNCATE = 1 << 4,
    PATH_DIRECTORY = 1 << 5
} path_flags_t;

typedef enum
{
    INODE_NONE = 0,
} inode_mode_t;

typedef struct
{
    map_entry_t entry;
    path_flags_t flag;
    const char* name;
} path_flag_entry_t;

typedef enum
{
    SUPER_NONE = 0,
} superblock_flags_t;

typedef struct filesystem
{
    list_entry_t entry;
    const char* name;
    superblock_t* (*mount)(const char* deviceName, superblock_flags_t flags, const void* data);
} filesystem_t;

typedef struct superblock
{
    list_entry_t entry;
    superblock_id_t id;
    atomic_uint64_t ref;
    uint64_t blockSize;
    uint64_t maxFileSize;
    superblock_flags_t flags;
    void* private;
    dentry_t* root;
    const super_ops_t* ops;
    const dentry_ops_t* dentryOps;
    char deviceName[MAX_NAME];
    char fsName[MAX_NAME];
    sysdir_t sysdir;
} superblock_t;

typedef struct inode
{
    map_entry_t mapEntry;
    inode_id_t id;
    atomic_uint64_t ref;
    inode_type_t type;
    inode_mode_t mode;
    uint64_t size;
    uint64_t blocks;
    uint64_t blockSize;
    clock_t accessTime;
    clock_t modifyTime;
    clock_t changeTime;
    uint64_t linkCount;
    void* private;
    superblock_t* superblock;
    const inode_ops_t* ops;
    const file_ops_t* fileOps;
} inode_t;

#define DETNRY_IS_ROOT(dentry) (dentry == dentry->parent)

typedef struct dentry
{
    map_entry_t mapEntry;
    dentry_id_t id;
    atomic_uint64_t ref;
    char name[MAX_NAME];
    inode_t* inode;
    dentry_t* parent;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    dentry_flags_t flags; // Protected by ::lock
    lock_t lock;
} dentry_t;

typedef struct file
{
    atomic_uint64_t ref;
    uint64_t pos;
    path_flags_t flags;
    dentry_t* dentry;
    const file_ops_t* ops;
    void* private;
} file_t;

typedef struct mount
{
    map_entry_t mapEntry;
    mount_id_t id;
    atomic_uint64_t ref;
    superblock_t* superblock;
    dentry_t* mountpoint;
    mount_t* parent;
} mount_t;

typedef struct poll_file
{
    file_t* file;
    poll_events_t events;
    poll_events_t revents;
} poll_file_t;

typedef struct superblock_ops
{
    inode_t* (*allocInode)(superblock_t* superblock);
    void (*freeInode)(superblock_t* superblock, inode_t* inode);
    uint64_t (*syncInode)(superblock_t* superblock, inode_t* inode);
    void (*cleanup)(superblock_t* superblock);
} super_ops_t;

typedef struct inode_ops
{
    dentry_t* (*lookup)(inode_t* dir, const char* name);
    inode_t* (*create)(inode_t* dir, const char* name, inode_mode_t mode);
} inode_ops_t;

typedef struct dentry_ops
{
    void (*cleanup)(dentry_t* entry);
} dentry_ops_t;

typedef struct file_ops
{    
    uint64_t (*open)(inode_t* inode, file_t* file);
    uint64_t (*open2)(inode_t* inode, file_t* files[2]);
    void (*cleanup)(file_t* file);
    uint64_t (*read)(file_t* file, void* buffer, uint64_t count);
    uint64_t (*write)(file_t* file, const void* buffer, uint64_t count);
    uint64_t (*seek)(file_t* file, int64_t offset, seek_origin_t origin);
    uint64_t (*ioctl)(file_t* file, uint64_t request, void* argp, uint64_t size);
    wait_queue_t* (*poll)(file_t* file, poll_file_t* pollFile);
    void* (*mmap)(file_t* file, void* address, uint64_t length, prot_t prot);
    uint64_t (*readdir)(file_t* file, stat_t* infos, uint64_t amount);
} file_ops_t;

typedef struct
{
    mount_t* mount;
    dentry_t* dentry;
} path_t;

typedef struct
{
    list_t list;
    rwlock_t lock;
} vfs_list_t;

typedef struct
{
    map_t map;
    rwlock_t lock;
} vfs_map_t;

typedef struct
{
    mount_t* mount;
    rwlock_t lock;
} vfs_root_t;

void vfs_init(void);

uint64_t vfs_register_fs(filesystem_t* fs);
uint64_t vfs_unregister_fs(filesystem_t* fs);
filesystem_t* vfs_get_filesystem(const char* name);

inode_t* vfs_get_inode(superblock_t* superblock, inode_id_t id);
dentry_t* vfs_get_dentry(dentry_t* parent, const char* name);

typedef struct
{
    char pathname[MAX_PATH];
    path_flags_t flags;
} parsed_pathname_t;

uint64_t vfs_parse_pathname(parsed_pathname_t* dest, const char* pathname);

uint64_t vfs_path_walk(path_t* outPath, const char* pathname, const path_t* start);
uint64_t vfs_path_walk_parent(path_t* outPath, const char* pathname, const path_t* start, char* outLastName);

uint64_t vfs_lookup(path_t* outPath, const char* pathname);
uint64_t vfs_lookup_parent(path_t* outPath, const char* pathname, char* outLastName);

uint64_t vfs_mount(const char* deviceName, const char* mountpoint, const char* fsName, superblock_flags_t flags,
    const void* data);
uint64_t vfs_unmount(const char* mountpoint);

file_t* vfs_open(const char* pathname);
uint64_t vfs_open2(const char* pathname, file_t* files[2]);
uint64_t vfs_read(file_t* file, void* buffer, uint64_t count);
uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count);
uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin);
uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size);
void* vfs_mmap(file_t* file, void* address, uint64_t length, prot_t prot);
uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout);

uint64_t vfs_readdir(file_t* file, stat_t* infos, uint64_t amount);
uint64_t vfs_mkdir(const char* pathname, uint64_t mode);
uint64_t vfs_rmdir(const char* pathname);

uint64_t vfs_stat(const char* pathname, stat_t* buffer);
uint64_t vfs_link(const char* oldpath, const char* newpath);
uint64_t vfs_unlink(const char* pathname);
uint64_t vfs_rename(const char* oldpath, const char* newpath);
uint64_t vfs_remove(const char* pathname);

bool vfs_is_name_valid(const char* name);

superblock_t* superblock_new(const char* deviceName, const char* fsName, super_ops_t* ops, dentry_ops_t* dentryOps);
void superblock_free(superblock_t* superblock);
superblock_t* superblock_ref(superblock_t* superblock);
void superblock_deref(superblock_t* superblock);

inode_t* inode_new(superblock_t* superblock, inode_type_t type, inode_ops_t* ops, file_ops_t* fileOps);
void inode_free(inode_t* inode);
inode_t* inode_ref(inode_t* inode);
void inode_deref(inode_t* inode);
uint64_t inode_sync(inode_t* inode);

dentry_t* dentry_new(dentry_t* parent, const char* name, inode_t* inode);
void dentry_free(dentry_t* dentry);
dentry_t* dentry_ref(dentry_t* dentry);
void dentry_deref(dentry_t* dentry);

file_t* file_new(dentry_t* dentry, path_flags_t flags);
void file_free(file_t* file);
file_t* file_ref(file_t* file);
void file_deref(file_t* file);

mount_t* mount_new(superblock_t* superblock, path_t* mountpoint);
void mount_free(mount_t* mount);
mount_t* mount_ref(mount_t* mount);
void mount_deref(mount_t* mount);

uint64_t path_get_root(path_t* outPath);
void path_copy(path_t* dest, const path_t* src);
void path_put(path_t* path);

static inline void superblock_defer_cleanup(superblock_t** superblock)
{
    if (*superblock != NULL)
    {
        superblock_deref(*superblock);
    }
}

static inline void inode_defer_cleanup(inode_t** inode)
{
    if (*inode != NULL)
    {
        inode_deref(*inode);
    }
}

static inline void dentry_defer_cleanup(dentry_t** entry)
{
    if (*entry != NULL)
    {
        dentry_deref(*entry);
    }
}

static inline void file_defer_cleanup(file_t** file)
{
    if (*file != NULL)
    {
        file_deref(*file);
    }
}

static inline void mount_defer_cleanup(mount_t** mount)
{
    if (*mount != NULL)
    {
        mount_deref(*mount);
    }
}

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}

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
