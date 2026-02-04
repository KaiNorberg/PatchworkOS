#include <kernel/fs/tmpfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/status.h>

static bool initialized = false;

static void tmpfs_read(irp_t* irp);
static void tmpfs_write(irp_t* irp);
static void tmpfs_seek(irp_t* irp);
static void tmpfs_truncate(vnode_t* vnode);

static void tmpfs_vnode_cleanup(vnode_t* vnode);

static vnode_class_t fileClass = {.name = "tmpfs file",
    .type = VNODE_REGULAR,
    .truncate = tmpfs_truncate,
    .cleanup = tmpfs_vnode_cleanup,
    .handlers = {
        [IRP_MJ_READ] = tmpfs_read,
        [IRP_MJ_WRITE] = tmpfs_write,
        [IRP_MJ_SEEK] = tmpfs_seek,
    }};

static status_t tmpfs_create(vnode_t* dir, dentry_t* target, mode_t mode);
static status_t tmpfs_link(vnode_t* dir, dentry_t* old, dentry_t* target);
static status_t tmpfs_symlink(vnode_t* dir, dentry_t* target, const char* dest);

static vnode_class_t dirClass = {
    .name = "tmpfs dir",
    .type = VNODE_DIR,
    .create = tmpfs_create,
    .link = tmpfs_link,
    .symlink = tmpfs_symlink,
    .cleanup = tmpfs_vnode_cleanup,
    .iterate = dentry_generic_iterate,
};

static status_t tmpfs_readlink(vnode_t* vnode, char* buffer, size_t count, size_t* bytesRead);

static vnode_class_t symlinkClass = {
    .name = "tmpfs symlink",
    .type = VNODE_SYMLINK,
    .readlink = tmpfs_readlink,
    .cleanup = tmpfs_vnode_cleanup,
};

static vnode_t* tmpfs_vnode_new(volume_t* volume, const vnode_class_t* cls, void* buffer, uint64_t size)
{
    vnode_t* vnode = vnode_new(volume, cls);
    if (vnode == NULL)
    {
        return NULL;
    }

    if (buffer != NULL)
    {
        vnode->data = malloc(size);
        if (vnode->data == NULL)
        {
            UNREF(vnode);
            return NULL;
        }
        memcpy(vnode->data, buffer, size);
        vnode->size = size;
    }
    else
    {
        vnode->data = NULL;
        vnode->size = 0;
    }

    return vnode;
}

static void tmpfs_dentry_add(dentry_t* dentry)
{
    tmpfs_volume_data_t* volume = dentry->volume->data;

    lock_acquire(&volume->lock);
    list_push_back(&volume->dentries, &dentry->entry);
    REF(dentry);
    lock_release(&volume->lock);
}

static void tmpfs_dentry_remove(dentry_t* dentry)
{
    tmpfs_volume_data_t* volume = dentry->volume->data;

    lock_acquire(&volume->lock);
    list_remove(&dentry->entry);
    UNREF(dentry);
    lock_release(&volume->lock);

    dentry_remove(dentry);
}

static void tmpfs_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    mutex_acquire(&frame->vnode->mutex);
    status_t status = mdl_copy_from_buffer(frame->read.buffer, frame->read.count, frame->read.offset, &irp->result,
        frame->vnode->data, frame->vnode->size);
    mutex_release(&frame->vnode->mutex);

    irp_complete(irp, status);
}

static void tmpfs_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    mutex_acquire(&frame->vnode->mutex);

    size_t required = frame->write.offset + frame->write.count;
    if (required > frame->vnode->size)
    {
        void* newData = realloc(frame->vnode->data, required);
        if (newData == NULL)
        {
            mutex_release(&frame->vnode->mutex);
            irp_complete(irp, ERR(FS, NOMEM));
            return;
        }
        memset((uint8_t*)newData + frame->vnode->size, 0, required - frame->vnode->size);
        frame->vnode->data = newData;
        frame->vnode->size = required;
    }

    status_t status = mdl_copy_to_buffer(frame->write.buffer, frame->write.count, frame->write.offset, &irp->result,
        frame->vnode->data, frame->vnode->size);
    mutex_release(&frame->vnode->mutex);

    irp_complete(irp, status);
}

static void tmpfs_seek(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->seek.file;

    mutex_acquire(&file->vnode->mutex);

    size_t pos;
    switch (frame->seek.origin)
    {
    case SEEK_SET:
        pos = frame->seek.offset;
        break;
    case SEEK_CUR:
        pos = atomic_load(&file->pos) + frame->seek.offset;
        break;
    case SEEK_END:
        pos = file->vnode->size + frame->seek.offset;
        break;
    default:
        mutex_release(&file->vnode->mutex);
        irp_complete(irp, ERR(FS, INVAL));
        return;
    }

    atomic_store(&file->pos, MIN(pos, file->vnode->size));
    irp->result = pos;

    mutex_release(&file->vnode->mutex);
    irp_complete(irp, OK);
}

static void tmpfs_truncate(vnode_t* vnode)
{
    MUTEX_SCOPE(&vnode->mutex);

    if (vnode->data != NULL)
    {
        free(vnode->data);
        vnode->data = NULL;
    }
    vnode->size = 0;
}

static status_t tmpfs_create(vnode_t* dir, dentry_t* target, mode_t mode)
{
    MUTEX_SCOPE(&dir->mutex);

    vnode_t* vnode = tmpfs_vnode_new(dir->volume, mode & MODE_DIRECTORY ? &dirClass : &fileClass, NULL, 0);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(target, vnode);
    tmpfs_dentry_add(target);

    return OK;
}

static status_t tmpfs_link(vnode_t* dir, dentry_t* old, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    dentry_make_positive(target, old->vnode);
    tmpfs_dentry_add(target);

    return OK;
}

static status_t tmpfs_symlink(vnode_t* dir, dentry_t* target, const char* dest)
{
    MUTEX_SCOPE(&dir->mutex);

    vnode_t* vnode = tmpfs_vnode_new(dir->volume, &symlinkClass, (void*)dest, strlen(dest));
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(target, vnode);
    tmpfs_dentry_add(target);

    return OK;
}

static status_t tmpfs_remove(vnode_t* dir, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    if (target->vnode->cls->type == VNODE_REGULAR || target->vnode->cls->type == VNODE_SYMLINK)
    {
        tmpfs_dentry_remove(target);
    }
    else if (target->vnode->cls->type == VNODE_DIR)
    {
        if (!list_is_empty(&target->children))
        {
            return ERR(FS, NOTEMPTY);
        }

        tmpfs_dentry_remove(target);
    }

    return OK;
}

static status_t tmpfs_readlink(vnode_t* vnode, char* buffer, size_t count, size_t* bytesRead)
{
    MUTEX_SCOPE(&vnode->mutex);

    if (vnode->data == NULL)
    {
        return ERR(FS, INVAL);
    }

    uint64_t copySize = MIN(count, vnode->size);
    memcpy(buffer, vnode->data, copySize);
    *bytesRead = copySize;
    return OK;
}

static void tmpfs_vnode_cleanup(vnode_t* vnode)
{
    if (vnode->data != NULL)
    {
        free(vnode->data);
        vnode->data = NULL;
        vnode->size = 0;
    }
}

static void tmpfs_volume_cleanup(volume_t* volume)
{
    UNUSED(volume);

    panic(NULL, "tmpfs unmounted\n");
}

static volume_ops_t volumeOps = {
    .cleanup = tmpfs_volume_cleanup,
};

static dentry_t* tmpfs_load_file(volume_t* volume, dentry_t* parent, const char* name, const boot_file_t* in)
{
    dentry_t* dentry = dentry_new(volume, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create tmpfs file dentry");
    }
    UNREF_DEFER(dentry);

    tmpfs_dentry_add(dentry);

    vnode_t* vnode = tmpfs_vnode_new(volume, &fileClass, in->data, in->size);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create tmpfs file vnode");
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(dentry, vnode);

    return REF(dentry);
}

static dentry_t* tmpfs_load_dir(volume_t* volume, dentry_t* parent, const char* name, const boot_dir_t* in)
{
    tmpfs_volume_data_t* superData = volume->data;

    dentry_t* dentry = dentry_new(volume, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create tmpfs dentry");
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = tmpfs_vnode_new(volume, &dirClass, NULL, 0);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create tmpfs vnode");
    }
    UNREF_DEFER(vnode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, vnode);

    boot_file_t* file;
    LIST_FOR_EACH(file, &in->files, entry)
    {
        UNREF(tmpfs_load_file(volume, dentry, file->name, file));
    }

    boot_dir_t* child;
    LIST_FOR_EACH(child, &in->children, entry)
    {
        UNREF(tmpfs_load_dir(volume, dentry, child->name, child));
    }

    return REF(dentry);
}

static status_t tmpfs_mount(filesystem_t* fs, dentry_t** out, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        return ERR(FS, INVAL);
    }

    volume_t* volume = volume_new(fs, &volumeOps);
    if (volume == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(volume);

    tmpfs_volume_data_t* tmpfsData = malloc(sizeof(tmpfs_volume_data_t));
    if (tmpfsData == NULL)
    {
        return ERR(FS, NOMEM);
    }
    list_init(&tmpfsData->dentries);
    lock_init(&tmpfsData->lock);
    volume->data = tmpfsData;

    if (!initialized)
    {
        boot_info_t* bootInfo = boot_info_get();
        const boot_disk_t* disk = &bootInfo->disk;

        dentry_t* root = tmpfs_load_dir(volume, NULL, NULL, disk->root);
        if (root == NULL)
        {
            return ERR(FS, NOMEM);
        }

        volume->root = root;
        *out = REF(volume->root);
        return OK;
    }

    dentry_t* dentry = dentry_new(volume, NULL, NULL);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = tmpfs_vnode_new(volume, &dirClass, NULL, 0);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, vnode);

    volume->root = dentry;
    *out = REF(volume->root);
    return OK;
}

static filesystem_t tmpfs = {
    .name = TMPFS_NAME,
    .mount = tmpfs_mount,
};

void tmpfs_init(void)
{
    LOG_INFO("registering tmpfs\n");
    if (IS_ERR(filesystem_register(&tmpfs)))
    {
        panic(NULL, "Failed to register tmpfs");
    }
    LOG_INFO("mounting tmpfs\n");

    process_t* process = process_current();
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        panic(NULL, "Failed to get process namespace");
    }
    UNREF_DEFER(ns);

    status_t status = namespace_mount(ns, NULL, &tmpfs, NULL, MODE_PROPAGATE | MODE_ALL_PERMS, NULL, NULL);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to mount tmpfs");
    }
    LOG_INFO("tmpfs initialized\n");

    initialized = true;
}
