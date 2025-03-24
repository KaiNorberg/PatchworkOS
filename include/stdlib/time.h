#ifndef _TIME_H
#define _TIME_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"
#include "_AUX/time_t.h"

time_t time(time_t* arg);

#if defined(__cplusplus)
}
#endif

#endif
