#pragma once

#include "encoding/data.h"
#include "encoding/expression.h"
#include "encoding/name.h"
#include "encoding/named.h"

#include "sync/mutex.h"

#include <stdint.h>

/**
 * @brief ACPI AML
 * @defgroup kernel_acpi_aml AML
 * @ingroup kernel_acpi
 *
 * ACPI AML is a procedural turing complete bytecode language used to describe the hardware configuration of a computer
 * system. A hardware manufacturer creates the bytecode to describe their hardware, and we, as the kernel, parse it. The
 * bytecode contains instructions that create namespaces and provide device information, but it does not output this
 * data, it's not like JSON or similar, instead AML itself expects a series of functions (e.g., for creating device
 * objects, namespaces, etc.) that it can call to directly create these structures.
 *
 * The parser works like a recursive descent parser. For example, according to the specification, the entire AML code
 * block is defined as `AMLCode := DefBlockHeader TermList`, since we have already read the header, we then just call
 * the `aml_term_list_read()` function. A termlist is defined as `TermList := Nothing | <termobj termlist>`, this is a
 * recursive definition, which we could flatten to `termobj termobj termobj ... Nothing`. So we now call the
 * `aml_term_obj_read()` function on each termobj. A termobj is defined as `TermObj := Object | StatementOpcode |
 * ExpressionOpcode` we then determine if this TermObj is an Object, StatementOpcode, or ExpressionOpcode and continue
 * down the chain until we finally have something to execute.
 *
 * This parsing structure makes the parser a more or less 1:1 replica of the specification, hopefully making it easier
 * to understand and maintain. But, it does also result in some overhead and redundant parsing, potentially hurting
 * performance, however i believe the benefits outweigh the costs.
 *
 * Throughout the documentation objects are frequently said to have a definition, a breakdown of how these
 * definitions are read can be found in section 20.1 of the ACPI specification.
 *
 * @{
 */

/**
 * @brief The current revision of the AML subsystem.
 *
 * This number as far as i can tell just needs to be larger than 2. But the ACPICA tests expect it to be greater or
 * equal 0x20140114 and less then or equal 0x20500000. So we just use a date code. There is no need to update this.
 */
#define AML_CURRENT_REVISION 0x20251010

/**
 * @brief Initialize the AML subsystem.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_init(void);

/**
 * @brief Parse an AML bytecode stream.
 *
 * The `aml_parse()` function parses and executes a AML bytestream, which creates the ACPI object tree.
 *
 * @param start Pointer to the start of the AML bytecode.
 * @param end Pointer to the end of the AML bytecode.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_parse(const uint8_t* start, const uint8_t* end);

/**
 * @brief Get the root object of the ACPI namespace.
 *
 * @return A reference to the root object.
 */
aml_object_t* aml_root_get(void);

/**
 * @brief Get the mutex for the entire AML subsystem.
 *
 * Must be held when interacting with any AML data structures.
 *
 * @return mutex_t* A pointer to the mutex.
 */
mutex_t* aml_big_mutex_get(void);

/** @} */
