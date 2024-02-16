#include "ram_disk.h"

#include "tty/tty.h"
#include "utils/utils.h"
#include "heap/heap.h"

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"

RamDirectory* ram_disk_traverse(Disk* disk, const char* path)
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
                return 0;
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

    return directory;
}

RamFile* ram_directory_find_file(RamDirectory* directory, const char* filename)
{
    RamFile* file = directory->firstFile;
    while (file != 0)
    {
        if (vfs_utils_compare_names(file->name, filename))
        {
            break;
        }
        else
        {
            file = file->next;
        }
    }

    return file;
}

Status ram_disk_open(Disk* disk, File** out, const char* path, uint64_t flags)
{    
    if (((flags & FILE_FLAG_READ) && disk->read == 0) ||
        ((flags & FILE_FLAG_WRITE) && disk->write == 0))
    {
        return STATUS_NOT_ALLOWED;
    }

    RamDirectory* directory = ram_disk_traverse(disk, path);
    if (directory == 0)
    {
        return STATUS_INVALID_PATH;
    }

    const char* filename = vfs_utils_basename(path);
    if (filename == 0)
    {
        return STATUS_INVALID_PATH;
    }

    RamFile* file = ram_directory_find_file(directory, filename);
    if (file == 0)
    {
        //TODO: Implement file creation
        return STATUS_INVALID_NAME;
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

    Disk* disk = disk_new("ram", root);
    disk->open = ram_disk_open;
    disk->close = ram_disk_close;
    disk->read = ram_disk_read;
    disk->seek = ram_disk_seek;

    Status status = vfs_mount(disk);
    if (status != STATUS_SUCCESS)
    {
        tty_print(statusToString[status]);
        tty_end_message(TTY_MESSAGE_ER);
    }

    tty_end_message(TTY_MESSAGE_OK);
}