#ifndef _ALLOCA_H
#define _ALLOCA_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include "_AUX/config.h"

#define alloca(size) __builtin_alloca(size)

#ifdef __cplusplus
}
#endif

#endif
