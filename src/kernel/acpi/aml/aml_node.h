#pragma once

#include "encoding/data.h"
#include "encoding/expression.h"
#include "encoding/name.h"
#include "encoding/named.h"

#include "fs/sysfs.h"
#include "sync/mutex.h"

#include <stdint.h>

/**
 * @brief ACPI AML Node
 * @defgroup kernel_acpi_aml_node Node
 * @ingroup kernel_acpi_aml
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
 *
 * A node is either an element in the ACPI namespace tree, or a method argument/local variable which does not exist in
 * the tree.
 */
typedef enum
{
    AML_NODE_NONE = 0,       //!< Invalid node type.
    AML_NODE_PREDEFINED,     //!< A predefined "Device" node, think of it as a directory.
    AML_NODE_PREDEFINED_GL,  //!< The predefined "Global Lock" object.
    AML_NODE_PREDEFINED_OS,  //!< The predefined "Operating System" object.
    AML_NODE_PREDEFINED_OSI, //!< The predefined "Operating System Interfaces" object.
    AML_NODE_PREDEFINED_REV, //!< The predefined "Revision" object.
    AML_NODE_DEVICE,         //!< A device node, can contain other devices, methods, fields, etc.
    AML_NODE_PROCESSOR,      //!< A processor node, deprecated in version 6.4 of the ACPI specification.
    AML_NODE_THERMAL_ZONE,   //!< A thermal zone node.
    AML_NODE_POWER_RESOURCE, //!< A power resource node.
    AML_NODE_OPREGION,       //!< An operation region node.
    AML_NODE_FIELD,          //!< A normal field node, used to access data in an operation region.
    AML_NODE_METHOD,         //!< A method node.
    AML_NODE_NAME,           //!< A named data object, this includes Local variables.
    AML_NODE_MUTEX,          //!< A mutex node.
    AML_NODE_INDEX_FIELD,    //!< An index field node, used to access data in a buffer using an index and data field.
    AML_NODE_BANK_FIELD,     //!< A bank field node.
    AML_NODE_BUFFER_FIELD,   //!< A buffer field node, used to access data in a buffer.
    AML_NODE_ARG,            //!< A method argument, does not exist in the namespace tree.
    AML_NODE_LOCAL,          //!< A method local variable, does not exist in the namespace tree.
    AML_NODE_MAX             //!< Maximum value for bounds checking.
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
    mutex_t lock;
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
            aml_node_t* indexNode;
            aml_node_t* dataNode;
            aml_field_flags_t flags;
            aml_bit_size_t bitOffset;
            aml_bit_size_t bitSize;
        } indexField;
        struct
        {
            aml_buffer_t* buffer;
            aml_bit_size_t bitSize;
            aml_bit_size_t bitIndex;
        } bufferField;
        struct
        {
            aml_data_object_t bankValue;
            aml_node_t* opregion;
            aml_node_t* bank;
            aml_field_flags_t flags;
            aml_bit_size_t bitOffset;
            aml_bit_size_t bitSize;
        } bankField;
    };
    sysfs_dir_t dir;
} aml_node_t;

/**
 * @brief Create a new ACPI node and add it to the parent's children list if a parent is provided.
 *
 * @param parent Pointer to the parent node, can be `NULL`.
 * @param name Name of the new node, must be `AML_NAME_LENGTH` chars long.
 * @param type Type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_new(aml_node_t* parent, const char* name, aml_node_type_t type);

/**
 * @brief Free an ACPI node and all its children.
 *
 * @param node Pointer to the node to free.
 */
void aml_node_free(aml_node_t* node);

/**
 * @brief Find a child node with the given name.
 *
 * @param parent Pointer to the parent node.
 * @param name Name of the child node to find, must be `AML_NAME_LENGTH` chars long.
 * @return On success, a pointer to the found child node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find_child(aml_node_t* parent, const char* name);

/**
 * @brief Add a new node at the location and with the name specified by the NameString.
 *
 * @param string The Namestring specifying the parent node.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param type The type of the new node.
 * @return aml_node_t* On success, a pointer to the new node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_add(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type);

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given NameString.
 *
 * A search through the ACPI namespace follows these rules:
 * - If the NameString starts with a root character (`\`), the search starts from the root node.
 * - If the NameString starts with one or more parent prefix characters (`^`), the search starts from the parent of the
 *    `start` node, moving up one level for each `^`.
 * - If the NameString does not start with a root or parent prefix character, the search starts from the `start` node.
 *    If `start` is `NULL`, the search starts from the root node.
 * - Attempt to find a matching name in the current namespace scope (the `start` node and its children).
 * - If the matching name is not found, move up to the parent node and repeat the search.
 * - This continues until either a match is found or the node does not have a parent (i.e., the root is reached).
 *
 * @see Section 5.3 of the ACPI specification for more details.
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
 * @brief Get the expected argument count for a method node.
 *
 * If the node is not a method or certain predefined objects, 0 is returned.
 *
 * @param node The node to get the expected argument count for.
 * @return On success, the expected argument count. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_get_expected_arg_count(aml_node_t* node);

/** @} */
