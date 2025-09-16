#pragma once

#include "encoding/data.h"
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
 * @{
 */

/**
 * @brief Name of the root ACPI node.
 */
#define AML_ROOT_NAME "ROOT"

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
    AML_NODE_DEVICE,
    AML_NODE_PROCESSOR,
    AML_NODE_THERMAL_ZONE,
    AML_NODE_POWER_RESOURCE,
    AML_NODE_OPREGION,
    AML_NODE_FIELD,
    AML_NODE_METHOD,
    AML_NODE_NAME,
    AML_NODE_MUTEX,
    AML_NODE_INDEX_FIELD,
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
    char name[AML_NAME_LENGTH + 1];
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
            aml_address_t offset;
            uint32_t size;
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
            aml_address_t offset;
            uint32_t size;
        } indexField;
    } data;
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
 * @brief Add a new node to the ACPI namespace.
 *
 * @param parent Pointer to the parent node, can be `NULL`.
 * @param name Name of the new node, must be `AML_NAME_LENGTH` chars long.
 * @param type Type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_add_node(aml_node_t* parent, const char* name, aml_node_type_t type);

/**
 * @brief Add a new node at the location and with the name specified by the NameString.
 *
 * @param string The Namestring specifying the parent node.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param type The type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_add_node_at_name_string(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type);

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
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given NameString.
 *
 * @param nameString The NameString to search for.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @return On success, a pointer to the found node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_find_node_name_string(const aml_name_string_t* nameString, aml_node_t* start);

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
