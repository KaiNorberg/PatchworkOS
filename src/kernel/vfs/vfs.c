#include "vfs.h"

#include <libc/string.h>

#include <lib-system.h>

#include "tty/tty.h"
#include "list/list.h"
#include "heap/heap.h"
#include "vfs/utils/utils.h"

static List* disks;

Disk* disk_new(const char* name, void* internal)
{
    Disk* disk = kmalloc(sizeof(Disk));
    memset(disk, 0, sizeof(Disk));

    strcpy(disk->name, name);
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

void vfs_init(void)
{
    tty_start_message("VFS initializing");

    disks = list_new();

    tty_end_message(TTY_MESSAGE_OK);
}

/*Status vfs_mount(Disk* disk)
{
    if (!vfs_utils_validate_name(disk->name))
    {
        return STATUS_INVALID_NAME;
    }

    ListEntry* entry = disks->first;
    while (entry != 0)
    {
        Disk* other = entry->data;

        if (strcmp(other->name, disk->name) == 0)
        {
            return STATUS_ALREADY_EXISTS;
        }

        entry = entry->next;
    }

    list_push(disks, disk);

    return STATUS_SUCCESS;
}

Status vfs_open(File** out, const char* path, uint64_t flags)
{
    if ((flags & ~FILE_FLAG_ALL) != 0)
    {
        return STATUS_INVALID_FLAG;
    }

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
        ListEntry* entry = disks->first;
        while (entry != 0)
        {
            Disk* disk = entry->data;

            if (strcmp(disk->name, name) == 0)
            {
                if (disk->open != 0)
                {
                    return disk->open(disk, out, p + 1, flags);
                }
                else
                {
                    return STATUS_NOT_ALLOWED;
                }
            }

            entry = entry->next;
        }
        
        return STATUS_INVALID_PATH;
    }
    else
    {
        //Todo: Implement disk directory accessing
    }

    return STATUS_FAILURE;
}

Status vfs_read(File* file, void* buffer, uint64_t length)
{
    if (file == 0)
    {
        return STATUS_DOES_NOT_EXIST;
    }

    if (file->disk->read != 0 && (file->flags & FILE_FLAG_READ))
    {
        return file->disk->read(file, buffer, length);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

Status vfs_write(File* file, const void* buffer, uint64_t length)
{
    if (file == 0)
    {
        return STATUS_DOES_NOT_EXIST;
    }

    if (file->disk->write != 0 && (file->flags & FILE_FLAG_WRITE))
    {
        return file->disk->write(file, buffer, length);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

Status vfs_close(File* file)
{
    if (file == 0)
    {
        return STATUS_DOES_NOT_EXIST;
    }

    if (file->disk->close != 0)
    {
        return file->disk->close(file);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}

Status vfs_seek(File* file, int64_t offset, uint64_t origin)
{
    if (file == 0)
    {
        return STATUS_DOES_NOT_EXIST;
    }

    if (file->disk->seek != 0)
    {
        return file->disk->seek(file, offset, origin);
    }
    else
    {
        return STATUS_NOT_ALLOWED;
    }
}*/