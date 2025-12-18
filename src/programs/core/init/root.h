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
 * @{
 */

void root_service_start(void);

/** @} */