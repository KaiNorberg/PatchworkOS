#include "ram_disk.h"

#include "tty/tty.h"
#include "utils/utils.h"

#include "vfs/vfs.h"
#include "vfs/generic_disk/generic_disk.h"

Status ram_disk_read(File* file, void* buffer, uint64_t length)
{
    FileNode const* fileNode = file->internal;
    RamFile const* ramFile = fileNode->internal;

    uint64_t i = 0;
    for (; i < length; i++)
    {
        if (file->position + i >= ramFile->size)
        {
            break;
        }

        ((uint8_t*)buffer)[i] = ramFile->data[file->position + i];
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

void ram_disk_load_directory(DirectoryNode* dirNode, RamDirectory const* dir)
{
    for (uint64_t i = 0; i < dir->fileAmount; i++)
    {        
        RamFile* ramFile = &(dir->files[i]);

        FileNode* fileNode = generic_disk_create_file(dirNode, ramFile->name);
        fileNode->read = ram_disk_read;
        fileNode->internal = ramFile;
    }

    for (uint64_t i = 0; i < dir->directoryAmount; i++)
    {
        RamDirectory const* childDir = &(dir->directories[i]);

        DirectoryNode* childNode = generic_disk_create_dir(dirNode, childDir->name);
        ram_disk_load_directory(childNode, childDir);
    }
}

void ram_disk_init(RamDirectory* root)
{
    tty_start_message("Ram Disk initializing");

    GenericDisk* disk = generic_disk_new();

    ram_disk_load_directory(disk->root, root);

    vfs_mount(disk->disk, "ram");

    tty_end_message(TTY_MESSAGE_OK);
}