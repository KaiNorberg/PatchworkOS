#include "vfs.h"

#include "tty/tty.h"
#include "list/list.h"
#include "heap/heap.h"

#include "vfs/utils/utils.h"

#include <libc/string.h>

static List* diskDirectory;

Disk* disk_new(void* internal)
{
    Disk* disk = kmalloc(sizeof(Disk));
    memset(disk, 0, sizeof(Disk));

    disk->internal = internal;

    return disk;
}

File* file_new(Disk* disk, void* internal, uint64_t flags)
{
    File* file = kmalloc(sizeof(File));
    memset(file, 0, sizeof(File));

    file->disk = disk;
    file->internal = internal;
    file->flags = flags;

    return file;
}

void file_free(File* file)
{
    kfree(file);
}

void vfs_init()
{
    tty_start_message("Virtual File System initializing");

    diskDirectory = list_new();

    tty_end_message(TTY_MESSAGE_OK);
}

Status vfs_mount(Disk* disk, const char* name)
{
    if (!vfs_utils_validate_name(name))
    {
        return STATUS_INVALID_NAME;
    }

    ListEntry* entry = diskDirectory->first;
    while (entry != 0)
    {
        DiskFile const* diskFile = entry->data;

        if (strcmp(diskFile->name, name) == 0)
        {
            return STATUS_ALREADY_EXISTS;
        }

        entry = entry->next;
    }

    DiskFile* newDiskFile = kmalloc(sizeof(DiskFile));
    newDiskFile->disk = disk;
    strcpy(newDiskFile->name, name);

    list_push(diskDirectory, newDiskFile);

    return STATUS_SUCCESS;
}

Status vfs_open(File** out, const char* path, uint64_t flags)
{
    if (!vfs_utils_validate_path(path))
    {
        return STATUS_INVALID_PATH;
    }

    char name[VFS_MAX_NAME_LENGTH];
    memset(name, 0, sizeof(char) * VFS_MAX_NAME_LENGTH);

    char* n = (char*)name;
    char* p = (char*)path;
    while (1)
    {
        if (*p == VFS_DISK_DELIMITER)
        {
            p++;
            break;
        }
        else
        {
            *n = *p;
        }

        n++;
        p++;
    }

    if (*p == VFS_NAME_DELIMITER)
    {
        ListEntry* entry = diskDirectory->first;
        while (entry != 0)
        {
            DiskFile const* diskFile = entry->data;

            if (strcmp(diskFile->name, name) == 0)
            {
                return diskFile->disk->open(diskFile->disk, out, p + 1, flags);
            }

            entry = entry->next;
        }
        
        return STATUS_INVALID_PATH;
    }
    else
    {
        //Todo: Implement disk directory accesing
    }

    return STATUS_FAILURE;
}

Status vfs_read(File* file, void* buffer, uint64_t length)
{
    if (file->read != 0)
    {
        return file->read(file, buffer, length);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

Status vfs_write(File* file, const void* buffer, uint64_t length)
{
    if (file->write != 0)
    {
        return file->write(file, buffer, length);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

Status vfs_close(File* file)
{
    if (file->disk->close != 0)
    {
        return file->disk->close(file);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

//Temporary
void vfs_seek(File* file, uint64_t position)
{
    file->position = position;
}