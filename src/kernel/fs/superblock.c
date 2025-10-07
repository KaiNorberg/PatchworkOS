#include "superblock.h"

#include "mem/heap.h"
#include "mem/pmm.h"
#include "vfs.h"

static void superblock_free(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    vfs_remove_superblock(superblock);

    if (superblock->ops != NULL && superblock->ops->cleanup != NULL)
    {
        superblock->ops->cleanup(superblock);
    }

    if (superblock->root != NULL)
    {
        DEREF(superblock->root);
    }

    heap_free(superblock);
}

superblock_t* superblock_new(filesystem_t* fs, const char* deviceName, superblock_ops_t* ops, dentry_ops_t* dentryOps)
{
    superblock_t* superblock = heap_alloc(sizeof(superblock_t), HEAP_NONE);
    if (superblock == NULL)
    {
        return NULL;
    }

    ref_init(&superblock->ref, superblock_free);
    list_entry_init(&superblock->entry);
    superblock->id = vfs_get_new_id();
    superblock->blockSize = PAGE_SIZE;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = SUPER_NONE;
    superblock->private = NULL;
    superblock->root = NULL;
    superblock->ops = ops;
    superblock->dentryOps = dentryOps;
    strncpy(superblock->deviceName, deviceName, MAX_NAME - 1);
    superblock->deviceName[MAX_NAME - 1] = '\0';
    superblock->fs = fs;
    // superblock::sysfsDir is exposed in vfs_mount
    return superblock;
}
