#ifndef _SYS_ATOMINT_H
#define _SYS_ATOMINT_H 1

#include <stdatomic.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"

/**
 * @brief Atomic wrappers around `stdint.h`.
 * @ingroup libstd
 * @defgroup libstd_sys_atomint Atomic integers
 *
 * The `sys/atomint.h` header stores atomic wrappers around the types defined in `stdint.h`.
 *
 */

typedef _Atomic(uint64_t) __atomic_uint64;
typedef _Atomic(uint32_t) __atomic_uint32;
typedef _Atomic(uint16_t) __atomic_uint16;
typedef _Atomic(uint8_t) __atomic_uint8;

typedef _Atomic(int64_t) __atomic_int64;
typedef _Atomic(int32_t) __atomic_int32;
typedef _Atomic(int16_t) __atomic_int16;
typedef _Atomic(int8_t) __atomic_int8;

/**
 * @brief Atomic uint64_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_uint64 atomic_uint64;

/**
 * @brief Atomic uint32_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_uint32 atomic_uint32;

/**
 * @brief Atomic uint16_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_uint16 atomic_uint16;

/**
 * @brief Atomic uint8_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_uint8 atomic_uint8;

/**
 * @brief Atomic int64_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_int64 atomic_int64;

/**
 * @brief Atomic int32_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_int32 atomic_int32;

/**
 * @brief Atomic int16_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_int16 atomic_int16;

/**
 * @brief Atomic int8_t.
 * @ingroup libstd_sys_atomint
 */
typedef __atomic_int8 atomic_int8;

#if defined(__cplusplus)
}
#endif

#endif
