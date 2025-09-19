#pragma once

#include "encoding/data.h"
#include "encoding/expression.h"
#include "encoding/name.h"
#include "encoding/named.h"

#include "fs/sysfs.h"
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
 * @brief Name of the root ACPI node.
 */
#define AML_ROOT_NAME "\\___"

/**
 * @brief Maximum length of an ACPI name.
 */
#define AML_NAME_LENGTH 4

/**
 * @brief ACPI node type.
 */
typedef enum
{
    AML_NODE_NONE = 0,
    AML_NODE_PREDEFINED,
    AML_NODE_PREDEFINED_GL,
    AML_NODE_PREDEFINED_OS,
    AML_NODE_PREDEFINED_OSI,
    AML_NODE_PREDEFINED_REV,
    AML_NODE_DEVICE,
    AML_NODE_PROCESSOR,
    AML_NODE_THERMAL_ZONE,
    AML_NODE_POWER_RESOURCE,
    AML_NODE_OPREGION,
    AML_NODE_FIELD,
    AML_NODE_METHOD,
    AML_NODE_NAME, //!< A named data object, this includes Local variables.
    AML_NODE_MUTEX,
    AML_NODE_INDEX_FIELD,
    AML_NODE_BANK_FIELD,
    AML_NODE_MAX
} aml_node_type_t;

/**
 * @brief ACPI node.
 * @struct aml_node_t
 */
typedef struct aml_node
{
    list_entry_t entry;
    aml_node_type_t type;
    list_t children;
    struct aml_node* parent;
    char segment[AML_NAME_LENGTH + 1];
    union {
        struct
        {
            aml_region_space_t space;
            aml_address_t offset;
            uint32_t length;
        } opregion;
        struct
        {
            aml_node_t* opregion;
            aml_field_flags_t flags;
            aml_bit_size_t bitOffset;
            aml_bit_size_t bitSize;
        } field;
        struct
        {
            aml_method_flags_t flags;
            aml_address_t start;
            aml_address_t end;
        } method;
        struct
        {
            aml_data_object_t object;
        } name;
        struct
        {
            mutex_t mutex;
            aml_sync_level_t syncLevel;
        } mutex;
        struct
        {
            aml_proc_id_t procId;
            aml_pblk_addr_t pblkAddr;
            aml_pblk_len_t pblkLen;
        } processor;
        struct
        {
            struct aml_node* indexNode;
            struct aml_node* dataNode;
            aml_field_flags_t flags;
            aml_bit_size_t bitOffset;
            aml_bit_size_t bitSize;
        } indexField;
    };
    sysfs_dir_t dir;
} aml_node_t;

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
 * @brief Get the expected argument count for a method node.
 *
 * If the node is not a method or certain predefined objects, 0 is returned.
 *
 * @param node The node to get the expected argument count for.
 * @return On success, the expected argument count. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_get_expected_arg_count(aml_node_t* node);

/**
 * @brief Compare two ACPI names for equality.
 *
 * Compares two AML names, ignoring trailing '_' characters.
 *
 * @param s1 Pointer to the first name.
 * @param s2 Pointer to the second name.
 * @return true if the names are equal, false otherwise.
 */
bool aml_is_name_equal(const char* s1, const char* s2);

/**
 * @brief Find a child node with the given name.
 *
 * @param parent Pointer to the parent node.
 * @param name Name of the child node to find, must be `AML_NAME_LENGTH` chars long.
 * @return On success, a pointer to the found child node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find_child(aml_node_t* parent, const char* name);

/**
 * @brief Add a new node to the ACPI namespace.
 *
 * @param parent Pointer to the parent node, can be `NULL`.
 * @param name Name of the new node, must be `AML_NAME_LENGTH` chars long.
 * @param type Type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_add(aml_node_t* parent, const char* name, aml_node_type_t type);

/**
 * @brief Add a new node at the location and with the name specified by the NameString.
 *
 * @param string The Namestring specifying the parent node.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param type The type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_add_at_name_string(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type);

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given NameString.
 *
 * @param nameString The NameString to search for.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @return On success, a pointer to the found node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find(const aml_name_string_t* nameString, aml_node_t* start);

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given path.
 *
 * The path is a null-terminated string with segments separated by dots (e.g., "DEV0.SUB0.METH").
 * A leading backslash indicates an absolute path from the root (e.g., "\DEV0.SUB0.METH").
 * A leading caret indicates a relative path from the start node's parent (e.g., "^SUB0.METH").
 *
 * @param path The path string to search for.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @return On success, a pointer to the found node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find_by_path(const char* path, aml_node_t* start);

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
