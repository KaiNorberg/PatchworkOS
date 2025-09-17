#pragma once

#include "data.h"

/**
 * @brief ACPI AML Arg Objecs Encoding
 * @defgroup kernel_acpi_aml Args
 * @ingroup kernel_acpi
 *
 * See section 20.2.6.1 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Maximum number of arguments that can be passed to a method.
 */
#define AML_MAX_ARGS 7

/**
 * @brief ACPI AML Argument Object.
 */
typedef aml_data_object_t aml_arg_t;

/** @} */
