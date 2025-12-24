#pragma once

/**
 * @brief Root Service.
 * @defgroup programs_init_root Root Service
 * @ingroup programs_init
 *
 * As the init process is the root of all processes in the system, it has complete access to all system resources. This
 * makes it the obvious choice to run the root service.
 *
 * @todo Write the docs for the root service
 *
 * @todo Implement proper password authentication.
 *
 * @{
 */

void root_start(void);

/** @} */