#ifndef _SYS_ARCH_H
#define _SYS_ARCH_H 1

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/NULL.h"
#include "_libstd/PAGE_SIZE.h"
#include "_libstd/clock_t.h"
#include "_libstd/config.h"
#include "_libstd/fd_t.h"

/**
 * @brief Architecture Specifics.
 * @ingroup libstd
 * @defgroup libstd_sys_arch Architecture Specifics
 *
 * @{
 */

/**
 * @brief Architecture specific thread data codes.
 * @typedef arch_op_t
 */
typedef enum
{
    ARCH_GET_FS = 0, ///< Get the FS base address.
    ARCH_SET_FS = 1, ///< Set the FS base address.
} arch_op_t;

/**
 * @brief System call for setting architecture specific thread data.
 *
 * @param op The operation to perform.
 * @param addr If getting data, a pointer to store the retrieved address. If setting data, the address to set.
 * @return An appropriate status value.
 */
static inline status_t arch_ctl(arch_op_t op, uintptr_t addr)
{
    return syscall2(SYS_ARCH_CTL, NULL, op, addr);
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
