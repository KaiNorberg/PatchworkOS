#pragma once

#include "encoding/data.h"
#include "encoding/name.h"
#include "encoding/named.h"

#include "fs/sysfs.h"
#include "sync/mutex.h"

#include <stdint.h>

typedef struct aml_term_arg_list aml_term_arg_list_t;

/**
 * @brief Node
 * @defgroup kernel_acpi_aml_node Node
 * @ingroup kernel_acpi_aml
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
 * @brief ACPI data types.
 * @enum aml_data_type_t
 *
 * @see Section 19.3.5 table 19.5 of the ACPI specification for more details.
 */
typedef enum
{
    AML_DATA_UNINITALIZED = 0,
    AML_DATA_BUFFER = 1 << 0,
    AML_DATA_BUFFER_FIELD = 1 << 1,
    AML_DATA_DEBUG_OBJECT = 1 << 2,
    AML_DATA_DEVICE = 1 << 3,
    AML_DATA_EVENT = 1 << 4,
    AML_DATA_FIELD_UNIT = 1 << 5,
    AML_DATA_INTEGER = 1 << 6,
    AML_DATA_INTEGER_CONSTANT = 1 << 7,
    AML_DATA_METHOD = 1 << 8,
    AML_DATA_MUTEX = 1 << 9,
    AML_DATA_OBJECT_REFERENCE = 1 << 10,
    AML_DATA_OPERATION_REGION = 1 << 11,
    AML_DATA_PACKAGE = 1 << 12,
    AML_DATA_POWER_RESOURCE = 1 << 13,
    AML_DATA_PROCESSOR = 1 << 14,
    AML_DATA_RAW_DATA_BUFFER = 1 << 15,
    AML_DATA_STRING = 1 << 16,
    AML_DATA_THERMAL_ZONE = 1 << 17,
    AML_DATA_UNRESOLVED = 1 << 18, //!< Used for forward references, not in the spec.
    AML_DATA_ALIAS = 1 << 19,      //!< Used to implement DefAlias, not in the spec.
    AML_DATA_ACTUAL_DATA =
        AML_DATA_INTEGER | AML_DATA_INTEGER_CONSTANT | AML_DATA_STRING | AML_DATA_BUFFER | AML_DATA_PACKAGE,
    AML_DATA_ALL = AML_DATA_BUFFER | AML_DATA_BUFFER_FIELD | AML_DATA_DEBUG_OBJECT | AML_DATA_DEVICE | AML_DATA_EVENT |
        AML_DATA_FIELD_UNIT | AML_DATA_INTEGER | AML_DATA_METHOD | AML_DATA_MUTEX | AML_DATA_OBJECT_REFERENCE |
        AML_DATA_OPERATION_REGION | AML_DATA_PACKAGE | AML_DATA_POWER_RESOURCE | AML_DATA_PROCESSOR |
        AML_DATA_RAW_DATA_BUFFER | AML_DATA_STRING | AML_DATA_THERMAL_ZONE | AML_DATA_UNRESOLVED,
    AML_DATA_TYPE_AMOUNT = 20,
} aml_data_type_t;

/**
 * @brief Flags for ACPI data types.
 * @enum aml_data_type_flags_t
 */
typedef enum
{
    /**
     * No flags.
     */
    AML_DATA_FLAG_NONE = 0,
    /**
     * Data type is considered "actual data", as in a integer, integer constant, string, buffer or package.
     *
     * This isent strictly defined anywhere, my interpretation is that "actual data",
     * refers to any data that can be retrieved from a DataObject (section 20.2.3).
     *
     * You could also define it as static data, as in not stored in some firmware register or similar.
     */
    AML_DATA_FLAG_IS_ACTUAL_DATA = 1 << 0,
    /**
     * Data type is a valid "Data Object" (section 19.6.101) and can be converted to "actual data".
     */
    AML_DATA_FLAG_DATA_OBJECT = 1 << 1,
    /**
     * Data type is a valid "non-Data Object" (section 19.6.102) and can not be converted to "actual data".
     */
    AML_DATA_FLAG_NON_DATA_OBJECT = 1 << 2,
} aml_data_type_flags_t;

/**
 * @brief Information about an ACPI data type.
 * @struct aml_data_type_info_t
 */
typedef struct
{
    const char* name;
    aml_data_type_t type;
    aml_data_type_flags_t flags;
} aml_data_type_info_t;

/**
 * @brief Flags for ACPI nodes.
 * @enum aml_node_flags_t
 */
typedef enum
{
    AML_NODE_NONE = 0,            //!< No flags.
    AML_NODE_ROOT = 1 << 0,       //!< Is the root node.
    AML_NODE_PREDEFINED = 1 << 1, //!< Is a predefined node.
    AML_NODE_LOCAL = 1 << 2,      //!< Is a local variable.
    AML_NODE_ARG = 1 << 3,        //!< Is a method argument.
    AML_NODE_NAMED =
        1 << 4, //!< Is a named node, as in it appears in the namespace tree. Will be set in `aml_node_new()`.
} aml_node_flags_t;

/**
 * @brief Field Unit types.
 * @enum aml_field_unit_type_t
 *
 * Since the ACPI spec does not diferenciate between "objects" of type Field, IndexField and BankField, instead just
 * calling them all FieldUnits, we use this enum to diferenciate between different FieldUnit types, even if it might
 * be cleaner to use aml_data_type_t for this.
 */
typedef enum
{
    AML_FIELD_UNIT_NONE,
    AML_FIELD_UNIT_FIELD,
    AML_FIELD_UNIT_INDEX_FIELD,
    AML_FIELD_UNIT_BANK_FIELD,
} aml_field_unit_type_t;

/**
 * @brief Method Implementation function type.
 * @typedef aml_method_implementation_t
 */
typedef uint64_t (*aml_method_implementation_t)(aml_node_t* method, aml_term_arg_list_t* args, aml_node_t* out);

/**
 * @brief ACPI node.
 * @struct aml_node_t
 *
 * A node can represent mode then just a node in the ACPI namespace tree, in practice, its everything.
 * It simply represents any readable or writable entity, this includes the result of operations.
 */
typedef struct aml_node
{
    list_entry_t entry;
    aml_data_type_t type;
    aml_node_flags_t flags;
    list_t children;
    struct aml_node* parent;
    char segment[AML_NAME_LENGTH + 1];
    bool isAllocated;
    union {
        struct
        {
            /**
             * Array of nodes of type AML_DATA_BUFFER_FIELD, one for each byte in the buffer, really only used for
             * IndexOp since it needs to return a reference to a BufferField.
             */
            struct aml_node* byteFields;
            uint8_t* content;
            uint64_t length;
        } buffer;
        struct
        {
            uint8_t* buffer;
            aml_bit_size_t bitOffset;
            aml_bit_size_t bitSize;
        } bufferField;
        struct
        {
            // Nothing.
        } device;
        struct
        {
            aml_field_unit_type_t type;     //!< The type of field unit.
            struct aml_node* indexNode;     //!< Used for IndexField.
            struct aml_node* dataNode;      //!< Used for IndexField.
            struct aml_node* opregion;      //!< Used for Field and BankField.
            uint64_t bankValue;             //!< Used for BankField.
            struct aml_node* bank;          //!< Used for BankField.
            aml_field_flags_t flags;        //!< Used for Field, IndexField and BankField.
            aml_bit_size_t bitOffset;       //!< Used for Field, IndexField and BankField.
            aml_bit_size_t bitSize;         //!< Used for Field, IndexField and BankField.
            aml_region_space_t regionSpace; //!< Used for Field, IndexField and BankField.
        } fieldUnit;
        struct
        {
            uint64_t value;
        } integer;
        struct
        {
            uint64_t value;
        } integerConstant;
        struct
        {
            /**
             * Pointer to the C function that will execute the method. Really just used to implement predefined the
             * predefined method _OSI. If `implementation` is `NULL`, the method is just a normal AML method.
             */
            aml_method_implementation_t implementation;
            aml_method_flags_t flags;
            const uint8_t* start;
            const uint8_t* end;
            mutex_t mutex;
        } method;
        struct
        {
            mutex_t mutex;
            aml_sync_level_t syncLevel;
        } mutex;
        struct
        {
            struct aml_node* target;
        } objectReference;
        struct
        {
            aml_region_space_t space;
            uint64_t offset;
            uint32_t length;
        } opregion;
        struct
        {
            uint64_t length;
            struct aml_node** elements;
        } package;
        struct
        {
            aml_proc_id_t procId;
            aml_pblk_addr_t pblkAddr;
            aml_pblk_len_t pblkLen;
        } processor;
        struct
        {
            /**
             * Array of nodes of type AML_DATA_BUFFER_FIELD, one for each char in the string, really only used for
             * IndexOp since it needs to return a reference to a BufferField.
             */
            struct aml_node* byteFields;
            char* content;
            uint64_t length;
        } string;
        /**
         * @brief Used for forward references.
         */
        struct
        {
            aml_name_string_t nameString;
            aml_node_t* start;
            aml_patch_up_resolve_callback_t callback;
        } unresolved;
        /**
         * @brief Used to implement DefAlias.
         */
        struct
        {
            struct aml_node* target;
        } alias;
    };
    sysfs_dir_t dir;
} aml_node_t;

/**
 * @brief Create a new ACPI node without allocating memory for it.
 *
 * Intended to be used for nodes that dont appear in the namespace tree. Use `aml_node_new()` to create nodes that
 * should appear in the namespace tree.
 *
 * When the node is no longer needed, it should be deinitialized using `aml_node_deinit()`. But never freed using
 * `aml_node_free()`.
 *
 * @param nodeFlags Flags for the new node.
 * @return The new node.
 */
#define AML_NODE_CREATE(nodeFlags) \
    (aml_node_t) \
    { \
        .entry = LIST_ENTRY_CREATE, .type = AML_DATA_UNINITALIZED, .flags = nodeFlags, .children = LIST_CREATE, \
        .parent = NULL, .segment = {'_', 'T', '_', '_', '\0'}, .isAllocated = false, .dir = {0} \
    }

/**
 * @brief Retrieve information about an ACPI data type.
 *
 * @param type The ACPI data type.
 * @return A pointer to the data type information structure, if the type is invalid a pointer to a structure with
 * `name = "Unknown"` and `type = AML_DATA_UNINITALIZED` is returned.
 */
aml_data_type_info_t* aml_data_type_get_info(aml_data_type_t type);

/**
 * @brief Allocate a new ACPI node and add it to the parent's children list if a parent is provided.
 *
 * @param parent Pointer to the parent node, can be `NULL`.
 * @param name Name of the new node, must not be longer then `AML_NAME_LENGTH`.
 * @param flags Flags for the new node.
 * @return On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_new(aml_node_t* parent, const char* name, aml_node_flags_t flags);

/**
 * @brief Free an ACPI node and all its children.
 *
 * @param node Pointer to the node to free.
 */
void aml_node_free(aml_node_t* node);

/**
 * @brief Add a new node at the location and with the name specified by the NameString.
 *
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param string The Namestring specifying the location and name of the new node.
 * @param flags Flags for the new node.
 * @return On success, a pointer to the new node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_add(aml_node_t* start, aml_name_string_t* string, aml_node_flags_t flags);

/**
 * @brief Initialize an ACPI node as a buffer with the given content.
 *
 * @param node Pointer to the node to initialize.
 * @param buffer Pointer to the buffer.
 * @param bytesToCopy Number of bytes to copy from `buffer` to the node, the rest will be zeroed.
 * @param length The total length of the buffer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_buffer(aml_node_t* node, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length);

/**
 * @brief Initialize an ACPI node as an empty buffer with the given length.
 *
 * @param node Pointer to the node to initialize.
 * @param length Length of the buffer will also be the capacity.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_buffer_empty(aml_node_t* node, uint64_t length);

/**
 * @brief Initialize an ACPI node as a buffer field with the given buffer, bit offset and bit size.
 *
 * @param node Pointer to the node to initialize.
 * @param buffer Pointer to the buffer content.
 * @param bitOffset Bit offset within the buffer.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_buffer_field(aml_node_t* node, uint8_t* buffer, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI node as a device or bus.
 *
 * @param node Pointer to the node to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_device(aml_node_t* node);

/**
 * @brief Initialize an ACPI node as a field unit of type Field.
 *
 * @param node Pointer to the node to initialize.
 * @param opregion Pointer to the operation region node.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_field_unit_field(aml_node_t* node, aml_node_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI node as a field unit of type IndexField.
 *
 * @param node Pointer to the node to initialize.
 * @param indexNode Pointer to the index node.
 * @param dataNode Pointer to the data node.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_field_unit_index_field(aml_node_t* node, aml_node_t* indexNode, aml_node_t* dataNode,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI node as a field unit of type BankField.
 *
 * @param node Pointer to the node to initialize.
 * @param opregion Pointer to the operation region node.
 * @param bank Pointer to the bank node.
 * @param bankValue Value to write to the bank node to select the bank structure.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_field_unit_bank_field(aml_node_t* node, aml_node_t* opregion, aml_node_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI node as an integer with the given value and bit width.
 *
 * @param node Pointer to the node to initialize.
 * @param value The integer value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_integer(aml_node_t* node, uint64_t value);

/**
 * @brief Initialize an ACPI node as an integer constant with the given value.
 *
 * @param node Pointer to the node to initialize.
 * @param value The integer constant value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_integer_constant(aml_node_t* node, uint64_t value);

/**
 * @brief Initialize an ACPI node as a method with the given flags and address range.
 *
 * @param node Pointer to the node to initialize.
 * @param flags Flags for the method.
 * @param start Pointer to the start of the method's AML bytecode.
 * @param end Pointer to the end of the method's AML bytecode.
 * @param implementation Pointer to a C function that will execute the method, or `NULL` if the method is a normal
 * AML method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_method(aml_node_t* node, aml_method_flags_t* flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation);

/**
 * @brief Initialize an ACPI node as a mutex with the given synchronization level.
 *
 * @param node Pointer to the node to initialize.
 * @param syncLevel The synchronization level of the mutex (0-15).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_mutex(aml_node_t* node, aml_sync_level_t syncLevel);

/**
 * @brief Initialize an ACPI node as an ObjectReference to the given target node.
 *
 * @param node Pointer to the node to initialize.
 * @param target Pointer to the target node the ObjectReference will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_object_reference(aml_node_t* node, aml_node_t* target);

/**
 * @brief Initialize an ACPI node as an operation region with the given space, offset, and length.
 *
 * @param node Pointer to the node to initialize.
 * @param space The address space of the operation region.
 * @param offset The offset within the address space.
 * @param length The length of the operation region.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_operation_region(aml_node_t* node, aml_region_space_t space, uint64_t offset, uint32_t length);

/**
 * @brief Initialize an ACPI node as a package with the given number of elements.
 *
 * @param node Pointer to the node to initialize.
 * @param length Number of elements the package will be able to hold.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_package(aml_node_t* node, uint64_t length);

/**
 * @brief Initialize an ACPI node as a processor with the given ProcID, PblkAddr, and PblkLen.
 *
 * @param node Pointer to the node to initialize.
 * @param procId The processor ID.
 * @param pblkAddr The pblk address.
 * @param pblkLen The length of the pblk.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_processor(aml_node_t* node, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen);

/**
 * @brief Initialize an ACPI node as a string with the given value.
 *
 * @param node Pointer to the node to initialize.
 * @param str Pointer to the string.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_string(aml_node_t* node, const char* str);

/**
 * @brief Initialize an ACPI node as an empty string with the given length.
 *
 * The string will be initalized with zero chars fol
 *
 * @param node Pointer to the node to initialize.
 * @param length Length of the string will also be the capacity.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_string_empty(aml_node_t* node, uint64_t length);

/**
 * @brief Initialize an ACPI node as an unresolved reference and adds it to the patch-up system.
 *
 * This is used for forward references, where a NameString refers to a node that has not yet been defined.
 *
 * @param node Pointer to the node to initialize.
 * @param nameString The NameString representing the path to the target node.
 * @param start The node to start the search from when resolving the reference.
 * @param callback Will be called when the reference is resolved.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_unresolved(aml_node_t* node, aml_name_string_t* nameString, aml_node_t* start,
    aml_patch_up_resolve_callback_t callback);

/**
 * @brief Initialize an ACPI node as an alias to the given target node.
 *
 * This is used to implement the DefAlias structure.
 *
 * @param node Pointer to the node to initialize.
 * @param target Pointer to the target node the alias will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_init_alias(aml_node_t* node, aml_node_t* target);

/**
 * @brief Deinitialize an ACPI node, freeing any resources associated with it and setting its type to
 * `AML_DATA_UNINITALIZED`.
 *
 * @param node Pointer to the node to deinitialize.
 */
void aml_node_deinit(aml_node_t* node);

/**
 * @brief Traverse alias nodes to find the target node.
 *
 * Will follow the alias chain until it reaches a non-alias node or a `NULL` pointer.
 *
 * @param node Pointer to the starting node.
 * @return A pointer to the target node, or `NULL` if the input node is `NULL`.
 */
aml_node_t* aml_node_traverse_alias(aml_node_t* node);

/**
 * @brief Find a child node with the given name.
 *
 * @param parent Pointer to the parent node.
 * @param name Name of the child node to find, must be `AML_NAME_LENGTH` chars long.
 * @return On success, a pointer to the found child node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find_child(aml_node_t* parent, const char* name);

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given path.
 *
 * The path is a null-terminated string with segments separated by dots (e.g., "DEV0.SUB0.METH").
 * A leading backslash indicates an absolute path from the root (e.g., "\DEV0.SUB0.METH").
 * A leading caret indicates a relative path from the start node's parent (e.g., "^SUB0.METH").
 *
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param path The path string to search for.
 * @return On success, a pointer to the found node. On failure, `NULL` and `errno` is set.
 */
aml_node_t* aml_node_find(aml_node_t* start, const char* path);

/**
 * @brief Store bits into a node at the specified bit offset and size.
 *
 * This function only works on the following node types:
 * - Integers
 * - Buffers
 * - Strings
 *
 * @param node Pointer to the node to store bits into.
 * @param value The bits to store (only the least significant `bitSize` bits are used).
 * @param bitOffset The bit offset within the node's data to start storing to.
 * @param bitSize The number of bits to store (up to 64 or the node's bit width).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_put_bits_at(aml_node_t* node, uint64_t value, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Retrieve bits from a node at the specified bit offset and size.
 *
 * This function only works on the following node types:
 * - Integers
 * - Buffers
 * - Strings
 *
 * @param node Pointer to the node to extract bits from.
 * @param bitOffset The bit offset within the node's data to start extracting from.
 * @param bitSize The number of bits to extract (up to 64 or the node's bit width).
 * @param out Pointer to a buffer where the extracted bits will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_node_get_bits_at(aml_node_t* node, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint64_t* out);

/** @} */
