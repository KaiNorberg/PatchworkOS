#include "ram_disk.h"

#include "tty/tty.h"
#include "utils/utils.h"
#include "heap/heap.h"

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"

Status ram_disk_open(Disk* disk, File** out, const char* path, uint64_t flags)
{
    RamDirectory* directory = disk->internal;
    const char* directoryName = vfs_utils_first_dir(path);
    while (directoryName != 0)
    {
        RamDirectory* child = directory->firstChild;
        while (1)
        {
            if (child == 0)
            {
                return STATUS_INVALID_PATH;
            }

            if (vfs_utils_compare_names(child->name, directoryName))
            {
                directory = child;
                break;
            }
            else
            {
                child = child->next;
            }
        }

        directoryName = vfs_utils_next_dir(directoryName);
    }

    const char* fileName = vfs_utils_basename(path);
    if (fileName == 0)
    {
        return STATUS_INVALID_PATH;
    }

    RamFile* file = directory->firstFile;
    while (1)
    {
        if (file == 0)
        {
            return STATUS_INVALID_NAME;
        }

        if (vfs_utils_compare_names(file->name, fileName))
        {
            break;
        }
        else
        {
            file = file->next;
        }
    }

    if (((flags & FILE_FLAG_READ) && disk->read == 0) ||
        ((flags & FILE_FLAG_WRITE) && disk->write == 0))
    {
        return STATUS_NOT_ALLOWED;
    }

    (*out) = file_new(disk, file, flags);

    return STATUS_SUCCESS;
}

Status ram_disk_close(File* file)
{
    kfree(file);

    return STATUS_SUCCESS;
}

Status ram_disk_read(File* file, void* buffer, uint64_t length)
{
    RamFile const* ramFile = file->internal;

    uint64_t i = 0;
    for (; i < length; i++)
    {
        if (file->position + i >= ramFile->size)
        {
            break;
        }

        ((uint8_t*)buffer)[i] = ((uint8_t*)ramFile->data)[file->position + i];
    }

    if (i == 0)
    {
        return STATUS_END_OF_FILE;
    }
    else
    {
        file->position += i;
        return STATUS_SUCCESS;
    }
}

Status ram_disk_seek(File* file, int64_t offset, uint64_t origin)
{
    RamFile const* ramFile = file->internal;

    switch (origin)
    {
    case FILE_SEEK_SET:
    {
        file->position = offset;
    }
    break;
    case FILE_SEEK_CUR:
    {
        file->position += offset;
    }
    break;
    case FILE_SEEK_END:
    {
        file->position = ramFile->size - offset;
    }
    break;
    }

    return STATUS_SUCCESS;
}

void ram_disk_init(RamDirectory* root)
{
    tty_start_message("Ram Disk initializing");

    Disk* disk = disk_new(root);
    disk->open = ram_disk_open;
    disk->close = ram_disk_close;
    disk->read = ram_disk_read;
    disk->seek = ram_disk_seek;

    vfs_mount(disk, "ram");

    tty_end_message(TTY_MESSAGE_OK);
}