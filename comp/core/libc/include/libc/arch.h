#ifndef _SYS_ARCH_H
#define _SYS_ARCH_H 1

#include <libc/syscall.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/NULL.h"
#include "_libc/PAGE_SIZE.h"
#include "_libc/clock_t.h"
#include "_libc/config.h"

/**
 * @brief Architecture Specifics.
 * @ingroup libc
 * @defgroup libc_arch Architecture Specifics
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
