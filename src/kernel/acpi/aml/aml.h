#pragma once

#include "aml_node.h"

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
 * the `aml_termlist_read()` function. A termlist is defined as `TermList := Nothing | <termobj termlist>`, this is a
 * recursive definition, which we could flatten to `termobj termobj termobj ... Nothing`. So we now call the
 * `aml_termobj_read()` function on each termobj. A termobj is defined as `TermObj := Object | StatementOpcode |
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
 * Primary sources:
 * - [lai library](https://github.com/managarm/lai)
 * - [ACPI Specification](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 *
 * @{
 */

/**
 * @brief Initialize the AML subsystem.
 *
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
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
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_parse(const void* data, uint64_t size);

/**
 * @brief Add a new node to the ACPI namespace.
 *
 * @param parent Pointer to the parent node, can be `NULL`.
 * @param name Name of the new node, must be `AML_NAME_LENGTH` chars long.
 * @param type Type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_add_node(aml_node_t* parent, const char* name, aml_node_type_t type);

/**
 * @brief Find an ACPI node by its path.
 *
 * The `aml_find_node()` function searches for an ACPI node relative to `start`, if `start` is NULL then always starts
 * at root. Paths use the ACPI format for example, `_SB.PCI0.LPCB.EC0`, you can use the `\\` prefix to start at the root
 * and the `^` prefix to go back up the tree. Note that the prefixes can only be attached at the very start of the path.
 *
 * @param path Path to the ACPI node.
 * @param start Start node for the search.
 * @return aml_node_t* On success, a pointer to the ACPI node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_find_node(const char* path, aml_node_t* start);

/**
 * @brief Get the root node of the ACPI namespace.
 *
 * @return aml_node_t* A pointer to the root node.
 */
aml_node_t* aml_root_get(void);

/**
 * @brief Print the ACPI namespace tree for debugging purposes.
 *
 * @param node Pointer to the node to start printing from.
 * @param depth Depth of the current node, used to indent the output.
 * @param isLast Whether the current node is the last child of its parent.
 */
void aml_print_tree(aml_node_t* node, uint32_t depth, bool isLast);

/** @} */
