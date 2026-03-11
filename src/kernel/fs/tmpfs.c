#include <kernel/fs/tmpfs.h>

#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/init/boot_info.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>
#include <kernel/sched/clock.h>

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
        tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);
        MUTEX_SCOPE(&vnode->vnode.mutex);
        vnode->size = 0;
        vnode->mtime = vnode->ctime = clock_epoch();
    }

    return OK;
}

static status_t tmpfs_regular_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);
    
    MUTEX_SCOPE(&vnode->vnode.mutex);

    vnode->atime = clock_epoch();

    return irp_read_helper(irp, vnode->buffer, vnode->size);
}

static status_t tmpfs_regular_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    if (frame->file->mode & MODE_APPEND)
    {
        *frame->write.offset = vnode->size;
    }

    size_t required = *frame->write.offset + mdl_size(frame->write.buffer);
    if (required > vnode->capacity)
    {
        size_t newCapacity = MAX(vnode->capacity * 2, required);
        void* newData = realloc(vnode->buffer, newCapacity);
        if (newData == NULL)
        {
            return ERR(FS, NOMEM);
        }
        memset((uint8_t*)newData + vnode->size, 0, newCapacity - vnode->size);
        vnode->buffer = newData;
        vnode->capacity = newCapacity;
    }

    if (required > vnode->size)
    {
        vnode->size = required;
    }

    vnode->mtime = vnode->ctime = clock_epoch();

    return irp_write_helper(irp, vnode->buffer, vnode->size);
}   

static status_t tmpfs_regular_seek(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    return irp_seek_helper(irp, vnode->size);
}

static status_t tmpfs_regular_attr(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    switch (frame->attr.attr)
    {
    case FILE_GET_SIZE:
        irp->result = vnode->size;
        return OK;
    case FILE_SET_SIZE:
        vnode->size = frame->attr.value;
        vnode->mtime = vnode->ctime = clock_epoch();
        return OK;
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
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);

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

    return mdl_copy_in(frame->query.buffer, sizeof(file_info_t), 0, &irp->result, &info, sizeof(file_info_t));
}

static status_t tmpfs_regular_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* vnode = VNODE_GET(frame->vnode, tmpfs_vnode_t);

    MUTEX_SCOPE(&vnode->vnode.mutex);

    free(vnode->buffer);
    vnode->buffer = NULL;
    vnode->size = 0;
    vnode->capacity = 0;

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

    tmpfs_vnode_t* tvnode = VNODE_GET(vnode, tmpfs_vnode_t);
    tvnode->volume = volume; // No ref, volume is kept alive by the root.
    tvnode->buffer = NULL;
    tvnode->size = 0;
    tvnode->capacity = 0;
    tvnode->atime = tvnode->mtime = tvnode->ctime = tvnode->btime = clock_epoch();
    tvnode->nlink = 1;

    return tvnode;
}

static status_t tmpfs_dir_create(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    tmpfs_vnode_t* dir = VNODE_GET(frame->vnode, tmpfs_vnode_t);
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

        tmpfs_vnode_t* vnode = VNODE_GET(file->path.dentry->vnode, tmpfs_vnode_t);
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

    tmpfs_vnode_t* vnode = 
        tmpfs_vnode_create(dir->volume, cls, vnode_hash(dir->vnode.number, target->name));
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    if (frame->create.mode & MODE_SYMLINK)
    {
        vnode->size = strlen(frame->create.payload);
        vnode->buffer = strdup(frame->create.payload);
        if (vnode->buffer == NULL)
        {
            UNREF(vnode->volume);
            return ERR(FS, NOMEM);
        }
        vnode->capacity = vnode->size;
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
    tmpfs_vnode_t* dir = VNODE_GET(frame->vnode, tmpfs_vnode_t);

    MUTEX_SCOPE(&dir->vnode.mutex);

    if (DENTRY_IS_TYPE(target, FILE_TYPE_DIRECTORY))
    {
        if (!list_is_empty(&target->children))
        {
            return ERR(FS, NOTEMPTY);
        }
    }

    MUTEX_SCOPE(&target->vnode->mutex);

    tmpfs_vnode_t* targetVnode = VNODE_GET(target->vnode, tmpfs_vnode_t);
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

static dentry_t* ramfsRoot = NULL;

static status_t ramfs_load_file(tmpfs_volume_t* volume, dentry_t* parent, const char* name, const boot_file_t* in)
{
    dentry_t* dentry = dentry_new(parent, name);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(dentry);

    tmpfs_vnode_t* vnode = tmpfs_vnode_create(volume, &regularClass, vnode_hash(parent->vnode->number, name));
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    vnode->size = in->size;
    vnode->capacity = in->size;
    vnode->buffer = malloc(in->size);
    if (vnode->buffer == NULL)
    {
        return ERR(FS, NOMEM);
    }
    memcpy(vnode->buffer, in->data, in->size);

    dentry_make_positive(dentry, &vnode->vnode);

    lock_acquire(&volume->lock);
    list_push_back(&volume->dentries, &dentry->entry);
    REF(dentry);
    lock_release(&volume->lock);

    return OK;
}

static status_t ramfs_load_dir(tmpfs_volume_t* volume, dentry_t* parent, const char* name, const boot_dir_t* in)
{
    dentry_t* dentry;
    if (parent == NULL)
    {
        dentry = dentry_new(NULL, NULL);
    }
    else
    {
        dentry = dentry_new(parent, name);
    }
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(dentry);

    file_number_t number = parent == NULL ? 0 : vnode_hash(parent->vnode->number, name);
    tmpfs_vnode_t* vnode = tmpfs_vnode_create(volume, &dirClass, number);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(dentry, &vnode->vnode);

    if (parent != NULL)
    {
        lock_acquire(&volume->lock);
        list_push_back(&volume->dentries, &dentry->entry);
        REF(dentry);
        lock_release(&volume->lock);
    }
    else
    {
        volume->rootVnode = &vnode->vnode;
        REF(volume);
    }

    boot_file_t* file;
    LIST_FOR_EACH(file, &in->files, entry)
    {
        status_t status = ramfs_load_file(volume, dentry, file->name, file);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    boot_dir_t* child;
    LIST_FOR_EACH(child, &in->children, entry)
    {
        status_t status = ramfs_load_dir(volume, dentry, child->name, child);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    if (parent == NULL)
    {
        ramfsRoot = REF(dentry);
    }

    return OK;
}

static status_t ramfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    return file_redirect(frame->file, ramfsRoot);
}

static vnode_class_t ramfsCloneClass = {
    .name = "ramfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = ramfs_clone_open,
        },
};

static filesystem_t ramfs = {
    .name = "ramfs",
    .clone = &ramfsCloneClass,
};

static void ramfs_init(void)
{
    tmpfs_volume_t* volume = tmpfs_volume_new();
    if (volume == NULL)
    {
        panic(NULL, "Failed to create ramfs volume");
    }
    UNREF_DEFER(volume);

    boot_info_t* bootInfo = boot_info_get();
    const boot_disk_t* disk = &bootInfo->disk;

    status_t status = ramfs_load_dir(volume, NULL, NULL, disk->root);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to load ramfs %Y", status);
    }

    status = filesystem_register(&ramfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register ramfs %Y", status);
    }
}

void tmpfs_init(void)
{
    status_t status = filesystem_register(&tmpfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register tmpfs %Y", status);
    }

    ramfs_init();
}
