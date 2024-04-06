#pragma once

#include <ctype.h>

#include "defs/defs.h"
#include "vfs/vfs.h"

#define VFS_NAME_SEPARATOR '/'
#define VFS_DRIVE_SEPARATOR ':'

#define VFS_LETTER_BASE 'A'
#define VFS_LETTER_AMOUNT ('Z' - 'A' + 1)

#define VFS_VALID_CHAR(ch) (isalnum(ch) || (ch) == '_' || (ch) == '.')
#define VFS_VALID_LETTER(letter) ((letter) >= 'A' && (letter) <= 'Z')

#define VFS_END_OF_NAME(ch) ((ch) == VFS_NAME_SEPARATOR || (ch) == '\0')

File* file_new(Drive* drive, void* context);

bool vfs_compare_names(const char* a, const char* b);

const char* vfs_first_dir(const char* path);

const char* vfs_next_dir(const char* path);

const char* vfs_next_name(const char* path);

const char* vfs_basename(const char* path);

uint64_t vfs_parent_dir(char* dest, const char* src);