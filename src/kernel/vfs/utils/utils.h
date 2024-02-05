#pragma once

#include <stdint.h>

uint8_t vfs_utils_validate_name(const char* name);

uint8_t vfs_utils_validate_path(const char* path);

const char* vfs_utils_first_dir(const char* path);

const char* vfs_utils_next_dir(const char* path);

const char* vfs_utils_basename(const char* path);

uint8_t vfs_utils_compare_names(const char* name1, const char* name2);
