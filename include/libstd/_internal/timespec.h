#ifndef _AUX_TIMESPEC_H
#define _AUX_TIMESPEC_H 1

#include "time_t.h"

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

#endif