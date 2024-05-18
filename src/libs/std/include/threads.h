#ifndef _THREADS_H
#define _THREADS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "_AUX/config.h"
#include "_AUX/timespec.h"

int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

#if defined(__cplusplus)
}
#endif

#endif