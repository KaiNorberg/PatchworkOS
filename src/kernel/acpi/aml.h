#pragma once

#include <stdint.h>

/**
 * @brief ACPI AML Parser
 * @defgroup kernel_acpi_aml_parse AML Parser
 * @ingroup kernel
 *
 * ACPI AML is a procedural turing complete bytecode language used to describe the hardware configuration of a computer
 * system. A hardware manufacturer creates this bytecode to describe their hardware, we then, as the kernel, parse this
 * bytecode, the bytecode contains instructions that create namespaces and provide device information. But it does not
 * output this data, its not like JSON or similar, instead AML itself expects a series of functions that it can call to
 * directly create these structures.
 *
 * Primary sources:
 * - (lai library)[https://github.com/managarm/lai]
 * - (ACPI Specification)[https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf]
 *
 * @{
 */

typedef struct aml_state aml_state_t;

/**
 * @brief AML Operation Handler
 * @typedef aml_handler_t
 */
typedef uint64_t (*aml_handler_t)(aml_state_t* state);

/**
 * @brief AML Operation Descriptor
 * @struct aml_op_t
 */
typedef struct
{
    const char* name;
    aml_handler_t handler;
} aml_op_t;

/**
 * @brief Parse an AML bytecode stream
 *
 * The `aml_parse()` function parses and executes a AML bytestream, which creates the ACPI namespaces in the acpi SysFS
 * group (the `/acpi/` directory).
 *
 * @param data Pointer to the AML bytecode stream.
 * @param size Size of the AML bytecode stream.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_parse(const void* data, uint64_t size);

/** @} */
