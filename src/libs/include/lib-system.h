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
    STATUS_SUCCESS = 0,
    STATUS_FAILURE = 1,
    STATUS_INVALID_NAME = 2,
    STATUS_INVALID_PATH = 3,
    STATUS_ALREADY_EXISTS = 4,
    STATUS_NOT_ALLOWED = 5,
    STATUS_END_OF_FILE = 6,
    STATUS_CORRUPT = 7,
    STATUS_INVALID_POINTER = 8,
    STATUS_INVALID_FLAG = 9,
    STATUS_DOES_NOT_EXIST = 10,
    STATUS_INSUFFICIENT_SPACE = 11,
} Status;

static const char* statusToString[] =
{
    "SUCCESS",
    "FAILURE",
    "INVALID_NAME",
    "INVALID_PATH",
    "ALREADY_EXISTS",
    "NOT_ALLOWED",
    "END_OF_FILE",
    "CORRUPT",
    "INVALID_POINTER",
    "INVALID_FLAG",
    "DOES_NOT_EXIST",
    "INSUFFICIENT_SPACE"
};

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

//========= PROCESS =========

void exit(Status status);

int64_t spawn(const char* path);

int64_t map(void* address, uint64_t length);

int64_t sys_pid();

int64_t sys_tid();

int64_t sys_test(const char* string);

//========= STATUS =========

Status status(void);

const char* status_string(void);

#if defined(__cplusplus)
} /* extern "C" */
#endif