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
 * nodes, namespaces, etc.) that it can call to directly create these structures.
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
 * @brief Initialize the AML subsystem.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_init(void);

/**
 * @brief Parse an AML bytecode stream
 *
 * The `aml_parse()` function parses and executes a AML bytestream, which creates the ACPI node tree.
 *
 * It can be confusing what exactly a namespace or node is, my recommendation is to not think about it to much.
 *
 * @param data Pointer to the AML bytecode stream.
 * @param size Size of the AML bytecode stream.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_parse(const void* data, uint64_t size);

/**
 * @brief Evaluate a node and retrieve the result.
 *
 * This functions behaviour depends on the node type, for example, if the node is a method it will execute the method
 * and retrieve the result, if the node is a field it will read the value stored in the field, etc.
 *
 * It is also responsible for potentialy acquiring the global lock, depending on the behaviour of the node.
 *
 * Note that args->argCount should always be zero for non method nodes, and if it is not zero an error will be returned.
 *
 * @param node The node to evaluate.
 * @param out Pointer to the buffer where the result of the evaluation will be stored.
 * @param args Pointer to the argument list, can be `NULL` if no arguments are to be passed.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate(aml_node_t* node, aml_data_object_t* out, aml_term_arg_list_t* args);

/**
 * @brief Store a data object in a node.
 *
 * @param node The node to store the data object in.
 * @param object The data object to store.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_store(aml_node_t* node, aml_data_object_t* object);

/**
 * @brief Get the root node of the ACPI namespace.
 *
 * @return aml_node_t* A pointer to the root node.
 */
aml_node_t* aml_root_get(void);

/**
 * @brief Get the global AML mutex.
 *
 * @return mutex_t* A pointer to the global AML mutex.
 */
mutex_t* aml_global_mutex_get(void);

/**
 * @brief Print the ACPI namespace tree for debugging purposes.
 *
 * @param node Pointer to the node to start printing from.
 * @param depth Depth of the current node, used to indent the output.
 * @param isLast Whether the current node is the last child of its parent.
 */
void aml_print_tree(aml_node_t* node, uint32_t depth, bool isLast);

/** @} */
