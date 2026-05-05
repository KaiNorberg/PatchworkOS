#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#ifdef __cplusplus
extern "C"
{
#endif

// https://pubs.opengroup.org/onlinepubs/007908799/xsh/systypes.h.html

#include "_libc/blkcnt_t.h"
#include "_libc/blksize_t.h"
#include "_libc/clockid_t.h"
#include "_libc/dev_t.h"
#include "_libc/fsblkcnt_t.h"
#include "_libc/fsfilcnt_t.h"
#include "_libc/gid_t.h"
#include "_libc/id_t.h"
#include "_libc/ino_t.h"
#include "_libc/key_t.h"
#include "_libc/mode_t.h"
#include "_libc/nlink_t.h"
#include "_libc/off_t.h"
#include "_libc/pid_t.h"
#include "_libc/size_t.h"
#include "_libc/ssize_t.h"
#include "_libc/suseconds_t.h"
#include "_libc/time_t.h"
#include "_libc/timer_t.h"
#include "_libc/uid_t.h"
#include "_libc/useconds_t.h"

typedef unsigned long pthread_t;
typedef struct
{
    int placeholder;
} pthread_attr_t;
typedef struct
{
    int placeholder;
} pthread_mutex_t;
typedef struct
{
    int placeholder;
} pthread_mutexattr_t;
typedef struct
{
    int placeholder;
} pthread_cond_t;
typedef struct
{
    int placeholder;
} pthread_condattr_t;
typedef struct
{
    int placeholder;
} pthread_rwlock_t;
typedef struct
{
    int placeholder;
} pthread_rwlockattr_t;
typedef int pthread_key_t;
typedef int pthread_once_t;

#ifdef __cplusplus
}
#endif

#endif
