#ifndef _SYS_SHARED_DATA_H
#define _SYS_SHARED_DATA_H 1

#include <errno.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// This is not yet implemented, more just idle mussings.

typedef struct
{
    clock_t uptime;
    clock_t processRuntime;
    errno_t lastError;
} shared_data_t;

#if defined(__cplusplus)
}
#endif

#endif
