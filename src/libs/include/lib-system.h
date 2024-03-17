#pragma once

#define SYSTEM_ERROR 0xFFFFFFFFFFFFFFFF //-1
#define SYSTEM_ERROR_PTR ((void*)0)

#define FILE_FLAG_CREATE (1 << 0)
#define FILE_FLAG_READ (1 << 1)
#define FILE_FLAG_WRITE (1 << 2)
#define FILE_FLAG_ALL (FILE_FLAG_CREATE | FILE_FLAG_READ | FILE_FLAG_WRITE)

#define FILE_SEEK_SET 0
#define FILE_SEEK_CUR 1
#define FILE_SEEK_END 2

#define STATUS_SUCCESS 0
#define STATUS_FAILURE 1
#define STATUS_INVALID_NAME 2
#define STATUS_INVALID_PATH 3
#define STATUS_ALREADY_EXISTS 4
#define STATUS_NOT_ALLOWED 5
#define STATUS_END_OF_FILE 6
#define STATUS_CORRUPT 7
#define STATUS_INVALID_POINTER 8
#define STATUS_INVALID_FLAG 9
#define STATUS_DOES_NOT_EXIST 10
#define STATUS_INSUFFICIENT_SPACE 11

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

//========= PROCESS =========

void sys_exit(uint64_t status);

uint64_t sys_spawn(const char* path);

void* sys_allocate(void* address, uint64_t length);

//========= STATUS =========

uint64_t sys_status(void);

const char* status_string(uint64_t status);

//========= TEST =========

uint64_t sys_test(const char* string);

#if defined(__cplusplus)
} /* extern "C" */
#endif