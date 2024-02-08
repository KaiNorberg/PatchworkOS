#pragma once

#include "list/list.h"

#include "vfs/vfs.h"

#include <stdint.h>

typedef struct
{
    char name[VFS_MAX_NAME_LENGTH];
    void* internal;
} FileNode;

typedef struct
{    
    char name[VFS_MAX_NAME_LENGTH];

    List* fileNodes;
    List* children;
} DirectoryNode;

typedef struct
{
    Disk* disk;
    DirectoryNode* root;
} GenericDisk;

GenericDisk* generic_disk_new();

FileNode* generic_disk_create_file(DirectoryNode* parent, const char* name);

DirectoryNode* generic_disk_create_dir(DirectoryNode* parent, const char* name);