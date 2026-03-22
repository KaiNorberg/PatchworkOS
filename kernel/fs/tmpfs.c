#include <kernel/fs/tmpfs.h>

#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/start/boot_info.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/status.h>

static cache_t cache = CACHE_CREATE(cache, "tmpfs_vnode", sizeof(tmpfs_vnode_t), CACHE_LINE, NULL, NULL);

static status_t tmpfs_regular_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    if (frame->file->mode & MODE_TRUNCATE)
    {
        tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);
        MUTEX_SCOPE(&vnode->vnode.mutex);
        pagevec_deinit(&vnode->pages);
        vnode->size = 0;
        vnode->mtime = vnode->ctime = clock_epoch();
    }

    return OK;
}

static status_t tmpfs_regular_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    vnode->atime = clock_epoch();

    size_t offset = *frame->read.offset;
    if (offset >= vnode->size)
    {
        irp->result = 0;
        return OK;
    }

    size_t remaining = vnode->size - offset;
    size_t copied = 0;

    status_t status = sglist_copy_in_pagevec(frame->read.buffer, remaining, 0, &copied, &vnode->pages, offset);
    if (IS_ERR(status))
    {
        return status;
    }

    *frame->read.offset += copied;
    irp->result = copied;

    return OK;
}

static status_t tmpfs_regular_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    if (frame->file->mode & MODE_APPEND)
    {
        *frame->write.offset = vnode->size;
    }

    size_t offset = *frame->write.offset;
    size_t toWrite = sglist_size(frame->write.buffer);
    if (toWrite == 0)
    {
        irp->result = 0;
        return OK;
    }

    size_t required = offset + toWrite;
    size_t requiredPages = BYTES_TO_PAGES(required);
    if (requiredPages > vnode->pages.amount)
    {
        status_t status = pagevec_resize(&vnode->pages, requiredPages);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    size_t copied = 0;
    status_t status = sglist_copy_out_pagevec(frame->write.buffer, toWrite, 0, &copied, &vnode->pages, offset);
    if (IS_ERR(status))
    {
        return status;
    }

    offset += copied;
    if (offset > vnode->size)
    {
        vnode->size = offset;
    }

    vnode->mtime = vnode->ctime = clock_epoch();

    *frame->write.offset = offset;
    irp->result = copied;
    return OK;
}

static status_t tmpfs_regular_seek(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    return irp_seek_helper(irp, vnode->size);
}

static status_t tmpfs_regular_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    size_t offset = frame->mmap.offset;
    size_t length = frame->mmap.length;

    if (offset % PAGE_SIZE != 0)
    {
        return ERR(FS, INVAL);
    }

    size_t pageOffset = offset / PAGE_SIZE;
    size_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        return ERR(FS, INVAL);
    }

    if (pageOffset + pageAmount > vnode->pages.amount)
    {
        size_t requiredPages = pageOffset + pageAmount;
        status_t status = pagevec_resize(&vnode->pages, requiredPages);
        if (IS_ERR(status))
        {
            return status;
        }

        if (offset + length > vnode->size)
        {
            vnode->size = offset + length;
        }
    }

    void* addr = frame->mmap.address;
    status_t status =
        vmm_map_shared(&irp->process->space, &addr, &vnode->pages.pfns[pageOffset], pageAmount, frame->mmap.flags);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = (uintptr_t)addr;
    return OK;
}

static status_t tmpfs_regular_attr(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    switch (frame->attr.attr)
    {
    case FILE_GET_SIZE:
        irp->result = vnode->size;
        return OK;
    case FILE_SET_SIZE:
    {
        size_t newSize = frame->attr.value;
        size_t newPages = BYTES_TO_PAGES(newSize);

        if (newPages != vnode->pages.amount)
        {
            status_t status = pagevec_resize(&vnode->pages, newPages);
            if (IS_ERR(status))
            {
                vnode->size = vnode->pages.amount * PAGE_SIZE;
                return status;
            }
        }

        if (newSize < vnode->size && newSize % PAGE_SIZE != 0)
        {
            size_t pageOffset = newSize % PAGE_SIZE;
            pagevec_fill(&vnode->pages, newSize, 0, PAGE_SIZE - pageOffset);
        }

        vnode->size = newSize;
        vnode->mtime = vnode->ctime = clock_epoch();
        return OK;
    }
    case FILE_GET_ATIME:
        irp->result = vnode->atime;
        return OK;
    case FILE_SET_ATIME:
        vnode->atime = frame->attr.value;
        return OK;
    case FILE_GET_MTIME:
        irp->result = vnode->mtime;
        return OK;
    case FILE_SET_MTIME:
        vnode->mtime = frame->attr.value;
        return OK;
    case FILE_GET_CTIME:
        irp->result = vnode->ctime;
        return OK;
    case FILE_SET_CTIME:
        vnode->ctime = frame->attr.value;
        return OK;
    case FILE_GET_BTIME:
        irp->result = vnode->btime;
        return OK;
    case FILE_SET_BTIME:
        vnode->btime = frame->attr.value;
        return OK;
    case FILE_GET_NLINK:
        irp->result = vnode->nlink;
        return OK;
    case FILE_GET_NUMBER:
        irp->result = vnode->vnode.number;
        return OK;
    case FILE_GET_VOLUME:
        irp->result = vnode->vnode.volume;
        return OK;
    case FILE_GET_TYPE:
        irp->result = vnode->vnode.cls->type;
        return OK;
    default:
        return ERR(FS, INVAL);
    }
}

static status_t tmpfs_regular_query(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    file_info_t info = {0};

    MUTEX_SCOPE(&vnode->vnode.mutex);

    info.size = vnode->size;
    info.mask |= FILE_MASK_SIZE;

    info.atime = vnode->atime;
    info.mask |= FILE_MASK_ATIME;

    info.mtime = vnode->mtime;
    info.mask |= FILE_MASK_MTIME;

    info.ctime = vnode->ctime;
    info.mask |= FILE_MASK_CTIME;

    info.btime = vnode->btime;
    info.mask |= FILE_MASK_BTIME;

    info.nlink = vnode->nlink;
    info.mask |= FILE_MASK_NLINK;

    info.type = vnode->vnode.cls->type;
    info.mask |= FILE_MASK_TYPE;

    info.volume = vnode->vnode.volume;
    info.mask |= FILE_MASK_VOLUME;

    info.number = vnode->vnode.number;
    info.mask |= FILE_MASK_NUMBER;

    strcpy(info.name, frame->file->path.dentry->name);
    info.mask |= FILE_MASK_NAME;

    return sglist_copy_in(frame->query.buffer, sizeof(file_info_t), 0, &irp->result, &info, sizeof(file_info_t));
}

static status_t tmpfs_regular_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    pagevec_deinit(&vnode->pages);
    vnode->size = 0;

    if (&vnode->vnode == vnode->volume->rootVnode)
    {
        UNREF(vnode->volume);
    }

    return OK;
}

static vnode_class_t regularClass = {
    .name = "tmpfs regular",
    .type = FILE_TYPE_REGULAR,
    .cache = &cache,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = tmpfs_regular_open,
            [IRP_MJ_READ] = tmpfs_regular_read,
            [IRP_MJ_WRITE] = tmpfs_regular_write,
            [IRP_MJ_SEEK] = tmpfs_regular_seek,
            [IRP_MJ_MMAP] = tmpfs_regular_mmap,
            [IRP_MJ_ATTR] = tmpfs_regular_attr,
            [IRP_MJ_QUERY] = tmpfs_regular_query,
            [IRP_MJ_RECLAIM] = tmpfs_regular_reclaim,
        },
};

static status_t tmpfs_dir_create(irp_t* irp);
static status_t tmpfs_dir_remove(irp_t* irp);

static vnode_class_t dirClass = {
    .name = "tmpfs dir",
    .type = FILE_TYPE_DIRECTORY,
    .cache = &cache,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_CREATE] = tmpfs_dir_create,
            [IRP_MJ_REMOVE] = tmpfs_dir_remove,
            [IRP_MJ_RECLAIM] = tmpfs_regular_reclaim,
        },
};

static vnode_class_t symlinkClass = {
    .name = "tmpfs symlink",
    .type = FILE_TYPE_SYMLINK,
    .cache = &cache,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = tmpfs_regular_open,
            [IRP_MJ_READ] = tmpfs_regular_read,
            [IRP_MJ_WRITE] = tmpfs_regular_write,
            [IRP_MJ_SEEK] = tmpfs_regular_seek,
            [IRP_MJ_ATTR] = tmpfs_regular_attr,
            [IRP_MJ_QUERY] = tmpfs_regular_query,
            [IRP_MJ_RECLAIM] = tmpfs_regular_reclaim,
        },
};

static tmpfs_vnode_t* tmpfs_vnode_create(tmpfs_volume_t* volume, const vnode_class_t* cls, file_number_t number)
{
    vnode_t* vnode = vnode_new(volume->id, cls, number);
    if (vnode == NULL)
    {
        return NULL;
    }

    tmpfs_vnode_t* tvnode = CONTAINER_OF(vnode, tmpfs_vnode_t, vnode);
    tvnode->volume = volume; // No ref, volume is kept alive by the root.
    pagevec_init(&tvnode->pages);
    tvnode->size = 0;
    tvnode->atime = tvnode->mtime = tvnode->ctime = tvnode->btime = clock_epoch();
    tvnode->nlink = 1;

    return tvnode;
}

static status_t tmpfs_dir_create(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* dir = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);
    dentry_t* target = frame->create.dentry;

    MUTEX_SCOPE(&dir->vnode.mutex);

    if (DENTRY_IS_POSITIVE(target))
    {
        return ERR(FS, EXIST);
    }

    const vnode_class_t* cls;
    if (frame->create.mode & MODE_DIRECTORY)
    {
        cls = &dirClass;
    }
    else if (frame->create.mode & MODE_SYMLINK)
    {
        cls = &symlinkClass;
    }
    else if (frame->create.mode & MODE_HARDLINK)
    {
        fd_t link;
        if (sscanf(frame->create.payload, "%lld", &link) != 1)
        {
            return ERR(FS, INVAL);
        }

        file_t* file = file_table_get(&irp->process->files, link);
        if (file == NULL)
        {
            return ERR(FS, INVAL);
        }

        if (file->path.dentry->vnode->cls != &regularClass)
        {
            return ERR(FS, INVAL);
        }

        tmpfs_vnode_t* vnode = CONTAINER_OF(file->path.dentry->vnode, tmpfs_vnode_t, vnode);
        MUTEX_SCOPE(&vnode->vnode.mutex);
        vnode->nlink++;

        dentry_make_positive(target, file->path.dentry->vnode);

        lock_acquire(&dir->volume->lock);
        list_push_back(&dir->volume->dentries, &target->entry);
        REF(target);
        lock_release(&dir->volume->lock);

        return OK;
    }
    else
    {
        cls = &regularClass;
    }

    tmpfs_vnode_t* vnode = tmpfs_vnode_create(dir->volume, cls, vnode_hash(dir->vnode.number, target->name));
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    if (frame->create.mode & MODE_SYMLINK)
    {
        size_t payloadLen = strlen(frame->create.payload);
        size_t reqPages = BYTES_TO_PAGES(payloadLen);

        if (reqPages > 0)
        {
            status_t status = pagevec_resize(&vnode->pages, reqPages);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        vnode->size = payloadLen;
        pagevec_write(&vnode->pages, 0, frame->create.payload, payloadLen);
    }

    dentry_make_positive(target, &vnode->vnode);

    lock_acquire(&dir->volume->lock);
    list_push_back(&dir->volume->dentries, &target->entry);
    REF(target);
    lock_release(&dir->volume->lock);

    dir->mtime = dir->ctime = clock_epoch();

    return OK;
}

static status_t tmpfs_dir_remove(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    dentry_t* target = frame->remove.dentry;
    tmpfs_vnode_t* dir = CONTAINER_OF(frame->vnode, tmpfs_vnode_t, vnode);

    MUTEX_SCOPE(&dir->vnode.mutex);

    if (DENTRY_IS_TYPE(target, FILE_TYPE_DIRECTORY))
    {
        if (!list_is_empty(&target->children))
        {
            return ERR(FS, NOTEMPTY);
        }
    }

    MUTEX_SCOPE(&target->vnode->mutex);

    tmpfs_vnode_t* targetVnode = CONTAINER_OF(target->vnode, tmpfs_vnode_t, vnode);
    dentry_remove(target);
    targetVnode->nlink--;

    lock_acquire(&dir->volume->lock);
    list_remove(&target->entry);
    UNREF(target);
    lock_release(&dir->volume->lock);

    dir->mtime = dir->ctime = clock_epoch();

    return OK;
}

static void tmpfs_volume_free(tmpfs_volume_t* volume)
{
    while (!list_is_empty(&volume->dentries))
    {
        dentry_t* dentry = CONTAINER_OF(list_pop_front(&volume->dentries), dentry_t, entry);
        UNREF(dentry);
    }
    free(volume);
}

static tmpfs_volume_t* tmpfs_volume_new(void)
{
    tmpfs_volume_t* volume = malloc(sizeof(tmpfs_volume_t));
    if (volume == NULL)
    {
        return NULL;
    }

    ref_init(&volume->ref, tmpfs_volume_free);
    volume->id = volume_new();
    volume->rootVnode = NULL;
    list_init(&volume->dentries);
    lock_init(&volume->lock);
    return volume;
}

static status_t tmpfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    tmpfs_volume_t* volume = tmpfs_volume_new();
    if (volume == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(volume);

    dentry_t* root = dentry_new(NULL, NULL);
    if (root == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(root);

    tmpfs_vnode_t* vnode = tmpfs_vnode_create(volume, &dirClass, 0);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    volume->rootVnode = &vnode->vnode;
    REF(volume); // Kept alive by root vnode

    dentry_make_positive(root, &vnode->vnode);

    return file_redirect(frame->file, root);
}

static vnode_class_t cloneClass = {
    .name = "tmpfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = tmpfs_clone_open,
        },
};

static filesystem_t tmpfs = {
    .name = TMPFS_NAME,
    .clone = &cloneClass,
};

void tmpfs_init(void)
{
    status_t status = filesystem_register(&tmpfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register tmpfs %Y", status);
    }
}
