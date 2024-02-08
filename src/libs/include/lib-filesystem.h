#pragma once

#define FILE_FLAG_CREATE (1 << 0)
#define FILE_FLAG_READ (1 << 1)
#define FILE_FLAG_WRITE (1 << 2)
#define FILE_FLAG_ALL (FILE_FLAG_CREATE | FILE_FLAG_READ | FILE_FLAG_WRITE)

#define FILE_SEEK_SET 0
#define FILE_SEEK_CUR 1
#define FILE_SEEK_END 2

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

int64_t open(const char* path, uint64_t flags);

int64_t close(uint64_t fd);

int64_t read(uint64_t fd, void* buffer, uint64_t length);

int64_t write(uint64_t fd, const void* buffer, uint64_t length);

int64_t seek(uint64_t fd, int64_t offset, uint64_t origin);

#if defined(__cplusplus)
} /* extern "C" */
#endif
