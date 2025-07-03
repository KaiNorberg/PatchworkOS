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

/*static sysdir_t root;
static rwlock_t lock;

static void syshdr_init(syshdr_t* header, const char* name, uint64_t type)
{
    node_init(&header->node, name, type);
    atomic_init(&header->hidden, false);
    atomic_init(&header->ref, 1);
}

static sysdir_t* sysdir_ref(sysdir_t* dir)
{
    atomic_fetch_add(&dir->header.ref, 1);
    return dir;
}

static void sysdir_deref(sysdir_t* dir)
{
    if (atomic_fetch_sub(&dir->header.ref, 1) <= 1)
    {
        if (dir->onFree != NULL)
        {
            dir->onFree(dir);
        }
    }
}

static sysfile_t* sysfile_ref(sysfile_t* sysfile)
{
    atomic_fetch_add(&sysfile->header.ref, 1);
    return sysfile;
}

static void sysfile_deref(sysfile_t* sysfile)
{
    if (atomic_fetch_sub(&sysfile->header.ref, 1) <= 1)
    {
        if (sysfile->onFree != NULL)
        {
            sysfile->onFree(sysfile);
        }

        sysdir_deref(sysfile->dir);
    }
}

static uint64_t sysfs_getdirent(file_t* file, stat_t* infos, uint64_t amount)
{
    RWLOCK_READ_DEFER(&lock);
    sysdir_t* sysdir = CONTAINER_OF(file->syshdr, sysdir_t, header);

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &sysdir->header.node.children, entry)
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

        sysfile_t* sysfile = sysfile_ref(CONTAINER_OF(node, sysfile_t, header.node));
        rwlock_read_release(&lock);

        if (sysfile->ops->open == NULL)
        {
            sysfile_deref(sysfile);
            errno = ENOSYS; return NULL;
        }

        file_t* file = sysfile->ops->open(volume, path, sysfile);
        if (file == NULL)
        {
            sysfile_deref(sysfile);
            return NULL;
        }

        file->syshdr = &sysfile->header; // Reference
        return file;
    }
    else
    {
        if (!(path->flags & PATH_DIRECTORY))
        {
            rwlock_read_release(&lock);
            errno = ENOTDIR; return NULL;
        }

        sysdir_t* sysdir = sysdir_ref(CONTAINER_OF(node, sysdir_t, header.node));
        rwlock_read_release(&lock);

        file_t* file = file_new(volume, path, PATH_DIRECTORY);
        if (file == NULL)
        {
            sysdir_deref(sysdir);
            return NULL;
        }
        file->ops = &dirOps;

        file->syshdr = &sysdir->header; // Reference
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
    sysfile_t* sysfile = sysfile_ref(CONTAINER_OF(node, sysfile_t, header.node)); // First ref

    if (sysfile->ops->open2 == NULL)
    {
        sysfile_deref(sysfile);
        errno = ENOSYS; return ERR;
    }

    if (sysfile->ops->open2(volume, path, sysfile, files) == ERR)
    {
        sysfile_deref(sysfile);
        return ERR;
    }

    files[0]->syshdr = &sysfile->header; // First ref
    files[1]->syshdr = &sysfile_ref(sysfile)->header;
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
        sysdir_t* dir = CONTAINER_OF(file->syshdr, sysdir_t, header);
        sysdir_deref(dir);
    }
    else
    {
        sysfile_t* sysfile = CONTAINER_OF(file->syshdr, sysfile_t, header);

        if (sysfile->ops->cleanup != NULL)
        {
            sysfile->ops->cleanup(sysfile, file);
        }

        sysfile_deref(sysfile);
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

static sysfs_namespace_t dev;

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

    assert(sysfs_namespace_init(&dev, "dev") != ERR);
}

void syfs_after_vfs_init(void)
{
    assert(vfs_register_fs(&sysfs) != ERR);
    assert(sysfs_namespace_mount(&dev, "/") != ERR);
    // assert(vfs_mount(VFS_DEVICE_NAME_NONE, "/", SYSFS_NAME, SUPER_NONE, NULL) != ERR);
}

uint64_t sysfs_namespace_init(sysfs_namespace_t* namespace, const char* name)
{
    if (namespace == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sysdir_init(&namespace->root, name, NULL) == ERR)
    {
        return ERR;
    }
}

void sysfs_namespace_deinit(sysfs_namespace_t* namespace)
{
    if (namespace == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    sysdir_deinit(&namespace->root, NULL);
}

uint64_t sysfs_namespace_mount(sysfs_namespace_t* namespace, const char* parent)
{
    if (namespace == NULL || parent == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char mountpoint[MAX_PATH];
    snprintf(mountpoint, MAX_PATH - 1, "%s%s", parent, namespace->name);

    vfs_mount(VFS_DEVICE_NAME_NONE, )
}

void sysfs_namespace_unmount(sysfs_namespace_t* namespace)
{

}