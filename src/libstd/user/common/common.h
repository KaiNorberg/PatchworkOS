#pragma once

/**
 * @brief Internal user space common functions.
 * @defgroup libstd_common_user User Space
 * @ingroup libstd_common
 *
 * While the rest of the standard library is shared between kernel and user space, this module contains code that will
 * only be used in user space.
 *
 * @{
 */

void _user_init(void);

/** @} */
