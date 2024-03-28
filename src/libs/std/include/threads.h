#ifndef _THREADS_H
#define _THREADS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "auxiliary/config.h"
#include "auxiliary/timespec.h"

int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

#if defined(__cplusplus)
}
#endif

#endif