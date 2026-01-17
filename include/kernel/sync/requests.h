#pragma once

#include <kernel/sync/request.h>

/**
 * @brief Kernel Request Implementations
 * @defgroup kernel_sync_requests Requests
 * @ingroup kernel_sync
 *
 * @{
 */

/**
 * @brief No-operation request structure.
 * @struct request_nop_t
 */
typedef struct request_nop
{
    REQUEST_COMMON(struct request_nop, uint64_t);
} request_nop_t;

bool request_nop_cancel(request_nop_t* request);

/** @} */