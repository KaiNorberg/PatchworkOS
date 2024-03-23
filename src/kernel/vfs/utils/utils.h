#pragma once

#include <stdint.h>
#include <ctype.h>

#define VFS_DISK_DELIMITER ':'
#define VFS_NAME_DELIMITER '/'

#define VFS_END_OF_NAME(ch) ((ch) == VFS_DISK_DELIMITER || (ch) == VFS_NAME_DELIMITER || (ch) == '\0')
#define VFS_VALID_CHAR(ch) (isalnum(ch) || (ch) == '_' || (ch) == '.')

uint8_t vfs_valid_name(const char* name);

uint8_t vfs_valid_path(const char* path);

uint8_t vfs_compare_names(const char* a, const char* b);