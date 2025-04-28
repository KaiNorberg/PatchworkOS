#ifndef _SYS_ATOMINT_H
#define _SYS_ATOMINT_H 1

#include <stdatomic.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"

typedef _Atomic(uint64_t) atomic_uint64;
typedef _Atomic(uint32_t) atomic_uint32;
typedef _Atomic(uint16_t) atomic_uint16;
typedef _Atomic(uint8_t) atomic_uint8;

typedef _Atomic(uint64_t) atomic_int64;
typedef _Atomic(uint32_t) atomic_int32;
typedef _Atomic(uint16_t) atomic_int16;
typedef _Atomic(uint8_t) atomic_int8;

#if defined(__cplusplus)
}
#endif

#endif
