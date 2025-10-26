#pragma once

/**
 * @brief User space only functions and definitions.
 * @defgroup libstd_user User Space
 * @addtogroup libstd
 *
 * While the rest of the standard library is shared between kernel and user space, this module contains code that will
 * only be used in user space.
 *
 * @{
 */

void _user_init(void);

/** @} */
