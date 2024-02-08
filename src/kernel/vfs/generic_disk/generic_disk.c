#include "generic_disk.h"

#include "heap/heap.h"
#include "tty/tty.h"

#include "vfs/vfs.h"
#include "vfs/utils/utils.h"
#include "vfs/generic_disk/generic_disk.h"

#include <libc/string.h>

Status generic_disk_open(Disk* disk, File** out, const char* path, uint64_t flags)
{
    DirectoryNode* dirNode = disk->internal;
    const char* dirName = vfs_utils_first_dir(path);
    while (dirName != 0)
    {
        ListEntry* entry = dirNode->children->first;
        while (1)
        {
            if (entry == 0)
            {
                return STATUS_INVALID_PATH;
            }

            DirectoryNode* child = entry->data;
            if (vfs_utils_compare_names(child->name, dirName))
            {
                dirNode = child;
                break;
            }

            entry = entry->next;
        }

        dirName = vfs_utils_next_dir(dirName);
    }

    const char* fileName = vfs_utils_basename(path);
    if (fileName == 0)
    {
        return STATUS_INVALID_PATH;
    }

    FileNode* fileNode;
    ListEntry* entry = dirNode->fileNodes->first;
    while (1)
    {
        if (entry == 0)
        {
            return STATUS_INVALID_NAME;
        }

        FileNode* foundFile = entry->data;
        if (vfs_utils_compare_names(foundFile->name, fileName))
        {
            fileNode = foundFile;
            break;
        }

        entry = entry->next;
    }

    if (((flags & FILE_FLAG_READ) && disk->read == 0) ||
        ((flags & FILE_FLAG_WRITE) && disk->write == 0))
    {
        return STATUS_NOT_ALLOWED;
    }

    (*out) = file_new(disk, fileNode, flags);

    return STATUS_SUCCESS;
}

Status generic_disk_close(File* file)
{
    file_free(file);

    return STATUS_SUCCESS;
}

GenericDisk* generic_disk_new()
{
    GenericDisk* genericDisk = kmalloc(sizeof(GenericDisk));

    DirectoryNode* root = kmalloc(sizeof(DirectoryNode));
    memset(root->name, 0, VFS_MAX_NAME_LENGTH);
    root->children = list_new();
    root->fileNodes = list_new();

    Disk* disk = disk_new(root);

    disk->open = generic_disk_open;
    disk->close = generic_disk_close;

    genericDisk->disk = disk;
    genericDisk->root = root;

    return genericDisk;
}

FileNode* generic_disk_create_file(DirectoryNode* parent, const char* name)
{
    FileNode* fileNode = kmalloc(sizeof(DirectoryNode));
    memset(fileNode, 0, sizeof(FileNode));

    strcpy(fileNode->name, name);

    list_push(parent->fileNodes, fileNode);

    return fileNode;
}

DirectoryNode* generic_disk_create_dir(DirectoryNode* parent, const char* name)
{
    DirectoryNode* dirNode = kmalloc(sizeof(DirectoryNode));
    strcpy(dirNode->name, name);
    dirNode->children = list_new();
    dirNode->fileNodes = list_new();

    list_push(parent->children, dirNode);

    return dirNode;
}