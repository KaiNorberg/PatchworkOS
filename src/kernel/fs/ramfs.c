#include "ramfs.h"

#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sysfs.h"
#include "vfs.h"

#include <boot/boot_info.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static ramfs_inode_t* root;

static _Atomic(inode_number_t) newNumber = ATOMIC_VAR_INIT(0);

static ramfs_inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, const char* name, void* data,
    uint64_t size);
static ramfs_inode_t* ramfs_load_dir(superblock_t* superblock, const ram_dir_t* in);

/*static uint64_t ramfs_getdirent(file_t* file, stat_t* infos, uint64_t amount)
{
    LOCK_DEFER(&lock);
    ram_dir_t* ramDir = CONTAINER_OF(file->private, ram_dir_t, node);

    uint64_t index = 0;
    uint64_t total = 0;

    node_t* child;
    LIST_FOR_EACH(child, &ramDir->node.children, entry)
    {
        stat_t info = {0};
        strcpy(info.name, child->name);
        info.type = child->type == RAMFS_FILE ? STAT_FILE : STAT_DIR;
        info.size = 0;

        getdirent_push(infos, amount, &index, &total, &info);
    }

    return total;
}

static file_ops_t dirOps = {
    .getdirent = ramfs_getdirent,
};

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    if (ramFile->data == NULL)
    {
        return 0;
    }

    return BUFFER_READ(file, buffer, count, ramFile->data, ramFile->size);
}

static uint64_t ramfs_write(file_t* file, const void* buffer, uint64_t count)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    if (file->flags & PATH_APPEND)
    {
        file->pos = ramFile->size;
    }

    if (file->pos + count >= ramFile->size)
    {
        void* newData = heap_realloc(ramFile->data, file->pos + count, HEAP_VMM);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + ramFile->size, 0, file->pos + count - ramFile->size);
        ramFile->data = newData;
        ramFile->size = file->pos + count;
    }

    memcpy(ramFile->data + file->pos, buffer, count);
    file->pos += count;
    return 0;
}

static uint64_t ramfs_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    LOCK_DEFER(&lock);
    ram_file_t* ramFile = CONTAINER_OF(file->private, ram_file_t, node);

    return BUFFER_SEEK(file, offset, origin, ramFile->size);
}

static file_ops_t fileOps = {
    .read = ramfs_read,
    .write = ramfs_write,
    .seek = ramfs_seek,
};

static file_t* ramfs_open(volume_t* volume, const path_t* path)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        if (!(path->flags & PATH_CREATE))
        {
            errno = ENOENT; return NULL;
        }

        const char* filename = path_last_name(path);
        if (filename == NULL)
        {
            errno = EINVAL; return NULL;
        }
        node_t* parent = path_traverse_node_parent(path, &root->node);
        if (parent == NULL)
        {
            errno = ENOENT; return NULL;
        }

        if (path->flags & PATH_DIRECTORY)
        {
            ram_dir_t* ramDir = heap_alloc(sizeof(ram_dir_t), HEAP_NONE);
            if (ramDir == NULL)
            {
                return NULL;
            }
            node_init(&ramDir->node, filename, RAMFS_DIR);
            ramDir->openedAmount = 0;
            node_push(parent, &ramDir->node);

            node = &ramDir->node;
        }
        else
        {
            ram_file_t* ramFile = heap_alloc(sizeof(ram_file_t), HEAP_NONE);
            if (ramFile == NULL)
            {
                return NULL;
            }
            node_init(&ramFile->node, filename, RAMFS_FILE);
            ramFile->size = 0;
            ramFile->data = NULL;
            ramFile->openedAmount = 0;
            node_push(parent, &ramFile->node);

            node = &ramFile->node;
        }
    }
    else if ((path->flags & PATH_CREATE) && (path->flags & PATH_EXCLUSIVE))
    {
        errno = EEXIST; return NULL;
    }
    else if ((path->flags & PATH_DIRECTORY) && node->type != RAMFS_DIR)
    {
        errno = ENOTDIR; return NULL;
    }
    else if (!(path->flags & PATH_DIRECTORY) && node->type != RAMFS_FILE)
    {
        errno = EISDIR; return NULL;
    }

    file_t* file = file_new(volume, path, PATH_CREATE | PATH_EXCLUSIVE | PATH_APPEND | PATH_TRUNCATE | PATH_DIRECTORY);
    if (file == NULL)
    {
        return NULL;
    }
    file->private = node;

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        ramFile->openedAmount++;

        file->ops = &fileOps;
        if (path->flags & PATH_TRUNCATE)
        {
            heap_free(ramFile->data);
            ramFile->data = NULL;
            ramFile->size = 0;
        }
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        ramDir->openedAmount++;
        file->ops = &dirOps;
    }

    return file;
}

static void ramfs_cleanup(volume_t* volume, file_t* file)
{
    LOCK_DEFER(&lock);

    node_t* node = file->private;

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        ramFile->openedAmount--;
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        ramDir->openedAmount--;
    }
}

static uint64_t ramfs_stat(volume_t* volume, const path_t* path, stat_t* stat)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        errno = ENOENT; return ERR;
    }

    strcpy(stat->name, node->name);
    stat->type = node->type == RAMFS_FILE ? STAT_FILE : STAT_DIR;
    stat->size = 0;

    return 0;
}

static uint64_t ramfs_rename(volume_t* volume, const path_t* oldpath, const path_t* newpath)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(oldpath, &root->node);
    if (node == NULL || node == &root->node)
    {
        errno = ENOENT; return ERR;
    }

    node_t* dest = path_traverse_node_parent(newpath, &root->node);
    if (dest == NULL)
    {
        errno = ENOENT; return ERR;
    }
    else if (dest->type != RAMFS_DIR)
    {
        errno = ENOTDIR; return ERR;
    }

    const char* newName = path_last_name(newpath);
    if (newName == NULL)
    {
        errno = EINVAL; return ERR;
    }
    strcpy(node->name, newName);
    node_remove(node);
    node_push(dest, node);
    return 0;
}

static uint64_t ramfs_remove(volume_t* volume, const path_t* path)
{
    LOCK_DEFER(&lock);

    node_t* node = path_traverse_node(path, &root->node);
    if (node == NULL)
    {
        errno = ENOENT; return ERR;
    }

    if (node->type == RAMFS_FILE)
    {
        ram_file_t* ramFile = CONTAINER_OF(node, ram_file_t, node);
        if (ramFile->openedAmount != 0)
        {
            errno = EBUSY; return ERR;
        }
        node_remove(&ramFile->node);
        heap_free(ramFile->data);
        heap_free(ramFile);
    }
    else
    {
        ram_dir_t* ramDir = CONTAINER_OF(node, ram_dir_t, node);
        if (ramDir->openedAmount != 0 || ramDir->node.childAmount != 0)
        {
            errno = EBUSY; return ERR;
        }
        node_remove(&ramDir->node);
        heap_free(ramDir);
    }

    return 0;
}

static volume_ops_t volumeOps = {
    .open = ramfs_open,
    .cleanup = ramfs_cleanup,
    .stat = ramfs_stat,
    .rename = ramfs_rename,
    .remove = ramfs_remove,
};

static uint64_t ramfs_mount(const char* label)
{
    return vfs_attach_simple(label, &volumeOps);
}

static fs_t ramfs = {
    .name = "ramfs",
    .mount = ramfs_mount,
};*/

static uint64_t file_getdirent(file_t* file, dirent_t* buffer, uint64_t amount)
{
    ramfs_inode_t* inode = CONTAINER_OF(file->dentry->inode, ramfs_inode_t, inode);
    LOCK_DEFER(&inode->inode.lock);

    getdirent_ctx_t ctx = {0};

    ramfs_inode_t* child;
    LIST_FOR_EACH(child, &inode->children, entry)
    {
        LOCK_DEFER(&child->inode.lock);
        getdirent_write(&ctx, buffer, amount, child->inode.number, child->inode.type, child->name);
    }

    return ctx.total;
}

static file_ops_t fileOps = {
    .getdirent = file_getdirent,
};

static inode_t* ramfs_lookup(inode_t* parent, const char* name)
{
    LOCK_DEFER(&parent->lock);

    if (parent->type != INODE_DIR)
    {
        LOG_WARN("ramfs_lookup: called using a non-directory inode.\n");
        errno = EINVAL;
        return NULL;
    }

    ramfs_inode_t* inode = CONTAINER_OF(parent, ramfs_inode_t, inode);

    ramfs_inode_t* child;
    LIST_FOR_EACH(child, &inode->children, entry)
    {
        LOCK_DEFER(&child->inode.lock);
        if (strcmp(child->name, name) == 0)
        {
            return &child->inode;
        }
    }

    errno = ENOENT;
    return NULL;
}

static inode_ops_t inodeOps = {
    .lookup = ramfs_lookup,
};

static dentry_ops_t dentryOps = {

};

static inode_t* ramfs_alloc_inode(superblock_t* superblock)
{
    return heap_alloc(sizeof(ramfs_inode_t), HEAP_NONE);
}

static void ramfs_free_inode(superblock_t* superblock, inode_t* inode)
{
    heap_free(CONTAINER_OF(inode, ramfs_inode_t, inode));
}

static void ramfs_superblock_cleanup(superblock_t* superblock)
{
    log_panic(NULL, "ramfs unmounted\n");
}

static superblock_ops_t superOps = {
    .allocInode = ramfs_alloc_inode,
    .freeInode = ramfs_free_inode,
    .cleanup = ramfs_superblock_cleanup,
};

static superblock_t* ramfs_mount(filesystem_t* fs, const char* deviceName, superblock_flags_t flags,
    const void* private)
{
    superblock_t* superblock = superblock_new(deviceName, SYSFS_NAME, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = flags;

    ramfs_inode_t* inode = ramfs_load_dir(superblock, private);
    if (inode == NULL)
    {
        superblock_deref(superblock);
        return NULL;
    }

    superblock->root = dentry_new(NULL, VFS_ROOT_ENTRY_NAME, &inode->inode);
    if (superblock->root == NULL)
    {
        inode_deref(&inode->inode);
        superblock_deref(superblock);
        return NULL;
    }

    return superblock;
}

static filesystem_t ramfs = {
    .name = RAMFS_NAME,
    .mount = ramfs_mount,
};

static ramfs_inode_t* ramfs_load_dir(superblock_t* superblock, const ram_dir_t* in)
{
    ramfs_inode_t* inode = ramfs_inode_new(superblock, INODE_DIR, in->node.name, NULL, 0);
    assert(inode != NULL);

    node_t* child;
    LIST_FOR_EACH(child, &in->node.children, entry)
    {
        if (child->type == RAMFS_DIR)
        {
            ram_dir_t* dir = CONTAINER_OF(child, ram_dir_t, node);

            list_push(&inode->children, &ramfs_load_dir(superblock, dir)->entry);
        }
        else if (child->type == RAMFS_FILE)
        {
            ram_file_t* file = CONTAINER_OF(child, ram_file_t, node);

            ramfs_inode_t* fileInode = ramfs_inode_new(superblock, INODE_FILE, file->node.name, file->data, file->size);
            assert(inode != NULL);

            list_push(&inode->children, &fileInode->entry);
        }
    }

    return inode;
}

static ramfs_inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, const char* name, void* data,
    uint64_t size)
{
    // Becouse of the ramfs_alloc_inode function, this allocated inode is actually a ramfs_inode_t.
    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNumber, 1), type, &inodeOps, &fileOps);
    if (inode == NULL)
    {
        return NULL;
    }

    inode->blocks = 0;
    inode->size = size;

    ramfs_inode_t* ramfsInode = CONTAINER_OF(inode, ramfs_inode_t, inode);
    list_entry_init(&ramfsInode->entry);
    list_init(&ramfsInode->children);
    ramfsInode->openedAmount = 0;
    strncpy(ramfsInode->name, name, MAX_NAME - 1);
    ramfsInode->name[MAX_NAME - 1] = '\0';

    if (data == NULL)
    {
        assert(size == 0); // Sanity check.
        ramfsInode->data = NULL;
        return ramfsInode;
    }

    ramfsInode->data = heap_alloc(size, HEAP_VMM);
    if (ramfsInode->data == NULL)
    {
        heap_free(ramfsInode);
        return NULL;
    }
    memcpy(ramfsInode->data, data, size);
    return ramfsInode;
}

void ramfs_init(ram_disk_t* disk)
{
    LOG_INFO("ramfs: init\n");

    assert(vfs_register_fs(&ramfs) != ERR);
    assert(vfs_mount(VFS_DEVICE_NAME_NONE, "/", RAMFS_NAME, SUPER_NONE, disk->root) != ERR);
}
