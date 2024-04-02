#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "array/array.h"
#include "sched/sched.h"
#include "vfs/utils/utils.h"

static Array* disks;

static FindResult vfs_find_disk_callback(void* element, void* context)
{
    Disk* disk = element;
    const char* name = context;

    return vfs_compare_names(disk->name, name) ? FIND_FOUND : FIND_NOT_FOUND;
}

static inline Disk* vfs_find_disk(const char* name)
{
    return array_find(disks, vfs_find_disk_callback, (char*)name);
}

static inline File* file_get(uint64_t fd)
{
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    if (table->files[fd] == NULL)
    {
        lock_release(&table->lock);
        return NULL;
    }

    File* file = table->files[fd];
    atomic_fetch_add(&file->ref, 1);

    lock_release(&table->lock);
    return file;
}

static inline void file_put(File* file)
{
    if (atomic_fetch_sub(&file->ref, 1) <= 1)
    {
        if (file->funcs->cleanup != NULL)
        {
            file->funcs->cleanup(file);
        }
        kfree(file);
    }
}

void file_table_init(FileTable* table)
{
    memset(table, 0, sizeof(FileTable));
    table->lock = lock_create();
}

void file_table_cleanup(FileTable* table)
{
    for (uint64_t i = 0; i < CONFIG_FILE_AMOUNT; i++)
    {    
        File* file = table->files[i];
        if (file != NULL)
        {
            file_put(file);
        }
    }
}

void vfs_init()
{
    tty_start_message("VFS initializing");

    disks = array_new();

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t vfs_mount(const char* name, void* context, Filesystem* fs)
{
    if (!vfs_valid_name(name))
    {
        return ERROR(ENAME);
    }

    if (vfs_find_disk(name) != 0)
    {
        return ERROR(EEXIST);
    }

    Disk* disk = kmalloc(sizeof(Disk));
    strcpy(disk->name, name);
    disk->context = context;
    disk->funcs = &fs->diskFuncs;
    disk->fs = fs;

    array_push(disks, disk);
    return 0;
}

uint64_t vfs_open(const char* path)
{
    if (!vfs_valid_path(path))
    {
        return ERROR(EPATH);
    }

    if (path[0] == '/')
    {
        path++;
    }
    else
    {
        return ERROR(EIMPL);
    }

    Disk* disk = vfs_find_disk(path);
    if (disk == NULL)
    {
        return ERROR(ENAME);
    }

    path = vfs_next_name(path);
    if (path == NULL)
    {
        return ERROR(EPATH);
    }

    File* file = kmalloc(sizeof(File));
    file->funcs = &disk->fs->fileFuncs;
    file->context = NULL;
    file->position = 0;
    file->ref = 1;

    if (disk->funcs->open(disk, file, path) == ERR)
    {
        kfree(file);
        return ERR;
    }

    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    for (uint64_t fd = 0; fd < CONFIG_FILE_AMOUNT; fd++)
    {
        if (table->files[fd] == NULL)
        {
            table->files[fd] = file;

            lock_release(&table->lock);
            return fd;
        }
    }

    lock_release(&table->lock);
    return ERROR(EMFILE);
}

uint64_t vfs_close(uint64_t fd)
{    
    FileTable* table = &sched_process()->fileTable;
    lock_acquire(&table->lock);

    if (fd >= CONFIG_FILE_AMOUNT || table->files[fd] == NULL)
    {
        lock_release(&table->lock);
        return ERROR(EBADF);
    }

    File* file = table->files[fd];
    table->files[fd] = NULL;
    file_put(file);

    lock_release(&table->lock);
    return 0;
}

uint64_t vfs_read(uint64_t fd, void* buffer, uint64_t count)
{    
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->funcs->read != NULL)
    {
        result = file->funcs->read(file, buffer, count);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_put(file);
    return result;
}

uint64_t vfs_write(uint64_t fd, const void* buffer, uint64_t count)
{
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->funcs->write != NULL)
    {
        result = file->funcs->write(file, buffer, count);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_put(file);
    return result;
}

uint64_t vfs_seek(uint64_t fd, int64_t offset, uint8_t origin)
{
    File* file = file_get(fd);
    if (file == NULL)
    {
        return ERROR(EBADF);
    }

    uint64_t result;
    if (file->funcs->seek != NULL)
    {
        result = file->funcs->seek(file, offset, origin);
    }
    else
    {
        result = ERROR(EACCES);
    }

    file_put(file);
    return result;
}