#ifndef _INTERNAL_ERR_H
#define _INTERNAL_ERR_H 1

#include <stdint.h>

/**
 * @brief Integer error value.
 * @ingroup libstd
 *
 * @deprecated This value is deprecated, the entire OS is being rewriten to use the new `status_t` system.s
 *
 */
#define _FAIL (UINT64_MAX)

#endif
