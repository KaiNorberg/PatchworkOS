#include "sysfs.h"

#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sync/rwlock.h"
#include "vfs.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/list.h>

/*static sysfs_dir_t root;
static rwlock_t lock;

static void syshdr_init(syshdr_t* header, const char* name, uint64_t type)
{
    node_init(&header->node, name, type);
    atomic_init(&header->hidden, false);
    atomic_init(&header->ref, 1);
}

static sysfs_dir_t* sysfs_dir_ref(sysfs_dir_t* dir)
{
    atomic_fetch_add(&dir->header.ref, 1);
    return dir;
}

static void sysfs_dir_deref(sysfs_dir_t* dir)
{
    if (atomic_fetch_sub(&dir->header.ref, 1) <= 1)
    {
        if (dir->onFree != NULL)
        {
            dir->onFree(dir);
        }
    }
}

static sysfs_file_t* sysfs_file_ref(sysfs_file_t* sysfs_file)
{
    atomic_fetch_add(&sysfs_file->header.ref, 1);
    return sysfs_file;
}

static void sysfs_file_deref(sysfs_file_t* sysfs_file)
{
    if (atomic_fetch_sub(&sysfs_file->header.ref, 1) <= 1)
    {
        if (sysfs_file->onFree != NULL)
        {
            sysfs_file->onFree(sysfs_file);
        }

        sysfs_dir_deref(sysfs_file->dir);
    }
}

static uint64_t sysfs_getdirent(file_t* file, stat_t* infos, uint64_t amount)
{
    RWLOCK_READ_DEFER(&lock);
    sysfs_dir_t* sysfs_dir = CONTAINER_OF(file->syshdr, sysfs_dir_t, header);

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &sysfs_dir->header.node.children, entry)
    {
        stat_t info = {0};
        strcpy(info.name, child->name);
        info.type = child->type == SYSFS_OBJ ? STAT_FILE : STAT_DIR;
        info.size = 0;

        getdirent_push(infos, amount, &index, &total, &info);
    }

    return total;
}

static file_ops_t dirOps = {
    .getdirent = sysfs_getdirent,
};

static file_t* sysfs_open(volume_t* volume, const path_t* path)
{
    rwlock_read_acquire(&lock);
    node_t* node = path_traverse_node(path, &root.header.node);
    if (node == NULL)
    {
        rwlock_read_release(&lock);
        errno = ENOENT; return NULL;
    }

    if (node->type == SYSFS_OBJ)
    {
        if (path->flags & PATH_DIRECTORY)
        {
            rwlock_read_release(&lock);
            errno = EISDIR; return NULL;
        }

        sysfs_file_t* sysfs_file = sysfs_file_ref(CONTAINER_OF(node, sysfs_file_t, header.node));
        rwlock_read_release(&lock);

        if (sysfs_file->ops->open == NULL)
        {
            sysfs_file_deref(sysfs_file);
            errno = ENOSYS; return NULL;
        }

        file_t* file = sysfs_file->ops->open(volume, path, sysfs_file);
        if (file == NULL)
        {
            sysfs_file_deref(sysfs_file);
            return NULL;
        }

        file->syshdr = &sysfs_file->header; // Reference
        return file;
    }
    else
    {
        if (!(path->flags & PATH_DIRECTORY))
        {
            rwlock_read_release(&lock);
            errno = ENOTDIR; return NULL;
        }

        sysfs_dir_t* sysfs_dir = sysfs_dir_ref(CONTAINER_OF(node, sysfs_dir_t, header.node));
        rwlock_read_release(&lock);

        file_t* file = file_new(volume, path, PATH_DIRECTORY);
        if (file == NULL)
        {
            sysfs_dir_deref(sysfs_dir);
            return NULL;
        }
        file->ops = &dirOps;

        file->syshdr = &sysfs_dir->header; // Reference
        return file;
    }
}

static uint64_t sysfs_open2(volume_t* volume, const path_t* path, file_t* files[2])
{
    RWLOCK_READ_DEFER(&lock);
    node_t* node = path_traverse_node(path, &root.header.node);
    if (node == NULL)
    {
        errno = ENOENT; return ERR;
    }
    else if (node->type != SYSFS_OBJ)
    {
        errno = EISDIR; return ERR;
    }
    else if (path->flags & PATH_DIRECTORY)
    {
        errno = EISDIR; return ERR;
    }
    sysfs_file_t* sysfs_file = sysfs_file_ref(CONTAINER_OF(node, sysfs_file_t, header.node)); // First ref

    if (sysfs_file->ops->open2 == NULL)
    {
        sysfs_file_deref(sysfs_file);
        errno = ENOSYS; return ERR;
    }

    if (sysfs_file->ops->open2(volume, path, sysfs_file, files) == ERR)
    {
        sysfs_file_deref(sysfs_file);
        return ERR;
    }

    files[0]->syshdr = &sysfs_file->header; // First ref
    files[1]->syshdr = &sysfs_file_ref(sysfs_file)->header;
    return 0;
}

static uint64_t sysfs_stat(volume_t* volume, const path_t* path, stat_t* stat)
{
    RWLOCK_READ_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root.header.node);
    if (node == NULL)
    {
        errno = ENOENT; return ERR;
    }

    strcpy(stat->name, node->name);
    stat->type = node->type == SYSFS_OBJ ? STAT_FILE : STAT_DIR;
    stat->size = 0;

    return 0;
}

static void sysfs_cleanup(volume_t* volume, file_t* file)
{
    if (file->syshdr->node.type == SYSFS_DIR)
    {
        sysfs_dir_t* dir = CONTAINER_OF(file->syshdr, sysfs_dir_t, header);
        sysfs_dir_deref(dir);
    }
    else
    {
        sysfs_file_t* sysfs_file = CONTAINER_OF(file->syshdr, sysfs_file_t, header);

        if (sysfs_file->ops->cleanup != NULL)
        {
            sysfs_file->ops->cleanup(sysfs_file, file);
        }

        sysfs_file_deref(sysfs_file);
    }
}

static volume_ops_t volumeOps = {
    .open = sysfs_open,
    .open2 = sysfs_open2,
    .stat = sysfs_stat,
    .cleanup = sysfs_cleanup,
};

static uint64_t sysfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps);
}

static fs_t sysfs = {
    .name = "sysfs",
    .mount = sysfs_mount,
};*/

static sysfs_group_t dev;

static inode_ops_t inodeOps = {

};

static dentry_ops_t dentryOps = {

};

static superblock_ops_t superOps = {

};

/*static superblock_t* sysfs_mount(const char* deviceName, superblock_flags_t flags, const void* data)
{
    errno = ENOSYS; return NULL;
    superblock_t* superblock = superblock_new(deviceName, SYSFS_NAME, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;

    inode_t* rootInode = inode_new(superblock, INODE_DIR, &inodeOps, NULL);
    if (rootInode == NULL)
    {
        superblock_deref(superblock);
        return NULL;
    }

    superblock->root = dentry_new(NULL, VFS_ROOT_ENTRY_NAME, rootInode);
    if (superblock->root == NULL)
    {
        inode_deref(rootInode);
        superblock_deref(superblock);
        return NULL;
    }

    return superblock;
}*/

static filesystem_t sysfs = {
    .name = SYSFS_NAME,
    //.mount = sysfs_mount,
};

void sysfs_init(void)
{
    LOG_INFO("sysfs: init\n");

    assert(sysfs_group_init(&dev) != ERR);
}

void syfs_after_vfs_init(void)
{
    assert(vfs_register_fs(&sysfs) != ERR);
    assert(sysfs_group_mount(&dev, "/dev") != ERR);
    // assert(vfs_mount(VFS_DEVICE_NAME_NONE, "/", SYSFS_NAME, SUPER_NONE, NULL) != ERR);
}

uint64_t sysfs_group_init(sysfs_group_t* ns)
{
    if (ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sysfs_dir_init(&ns->root, "", NULL) == ERR)
    {
        return ERR;
    }

    memset(ns->mountpoint, 0, MAX_PATH);
}

void sysfs_group_deinit(sysfs_group_t* ns)
{
    if (ns == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    sysfs_dir_deinit(&ns->root, NULL);
}

uint64_t sysfs_group_mount(sysfs_group_t* ns, const char* mountpoint)
{
    if (ns == NULL || mountpoint == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    strncpy(ns->mountpoint, mountpoint, MAX_PATH - 1);
    ns->mountpoint[MAX_PATH - 1] = '\0';

    if (vfs_mount(VFS_DEVICE_NAME_NONE, ns->mountpoint, SYSFS_NAME, SUPER_NONE, ns) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t sysfs_group_unmount(sysfs_group_t* ns)
{
    return vfs_unmount(ns->mountpoint);
}