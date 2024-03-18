#ifndef _SYS_STATUS_H
#define _SYS_STATUS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#define SYSTEM_ERROR 0xFFFFFFFFFFFFFFFF //-1
#define SYSTEM_ERROR_PTR ((void*)0)

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
}
#endif
 
#endif