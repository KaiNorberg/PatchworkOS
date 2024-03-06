#pragma once

#define FILE_FLAG_CREATE (1 << 0)
#define FILE_FLAG_READ (1 << 1)
#define FILE_FLAG_WRITE (1 << 2)
#define FILE_FLAG_ALL (FILE_FLAG_CREATE | FILE_FLAG_READ | FILE_FLAG_WRITE)

#define FILE_SEEK_SET 0
#define FILE_SEEK_CUR 1
#define FILE_SEEK_END 2

typedef enum Status
{
    STATUS_SUCCESS,
    STATUS_FAILURE,
    STATUS_INVALID_NAME,
    STATUS_INVALID_PATH,
    STATUS_ALREADY_EXISTS,
    STATUS_NOT_ALLOWED,
    STATUS_END_OF_FILE,
    STATUS_CORRUPT,
    STATUS_INVALID_POINTER,
    STATUS_INVALID_FLAG,
    STATUS_DOES_NOT_EXIST,
    STATUS_INSUFFICIENT_SPACE,
} Status;

static const char* statusToString[] =
{
    [STATUS_SUCCESS] = "SUCCESS",
    [STATUS_FAILURE] = "FAILURE",
    [STATUS_INVALID_NAME] = "INVALID_NAME",
    [STATUS_INVALID_PATH] = "INVALID_PATH",
    [STATUS_ALREADY_EXISTS] = "ALREADY_EXISTS",
    [STATUS_NOT_ALLOWED] = "NOT_ALLOWED",
    [STATUS_END_OF_FILE] = "END_OF_FILE",
    [STATUS_CORRUPT] = "CORRUPT",
    [STATUS_INVALID_POINTER] = "INVALID_POINTER",
    [STATUS_INVALID_FLAG] = "INVALID_FLAG",
    [STATUS_DOES_NOT_EXIST] = "DOES_NOT_EXIST"
};

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

//========= FILESYSTEM =========

int64_t open(const char* path, uint64_t flags);

int64_t close(uint64_t fd);

int64_t read(uint64_t fd, void* buffer, uint64_t length);

int64_t write(uint64_t fd, const void* buffer, uint64_t length);

int64_t seek(uint64_t fd, int64_t offset, uint64_t origin);

//========= PROCESS =========

void exit(Status status);

int64_t spawn(const char* path);

int64_t sleep(uint64_t milliseconds);

int64_t map(void* lower, void* upper);

int64_t sys_test(const char* string);

//========= STATUS =========

Status status();

const char* status_string();

#if defined(__cplusplus)
} /* extern "C" */
#endif