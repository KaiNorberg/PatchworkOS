#pragma once

#include "encoding/data.h"
#include "encoding/name.h"
#include "encoding/named.h"

#include "fs/sysfs.h"
#include "sync/mutex.h"

#include <stdint.h>

typedef struct aml_term_arg_list aml_term_arg_list_t;

/**
 * @brief Object
 * @defgroup kernel_acpi_aml_object Object
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Name of the root ACPI object.
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
 * @brief Flags for ACPI objects.
 * @enum aml_object_flags_t
 */
typedef enum
{
    AML_OBJECT_NONE = 0,            //!< No flags.
    AML_OBJECT_ROOT = 1 << 0,       //!< Is the root object.
    AML_OBJECT_PREDEFINED = 1 << 1, //!< Is a predefined object.
    AML_OBJECT_LOCAL = 1 << 2,      //!< Is a local variable.
    AML_OBJECT_ARG = 1 << 3,        //!< Is a method argument.
    AML_OBJECT_NAMED =
        1 << 4, //!< Is a named object, as in it appears in the namespace tree. Will be set in `aml_object_new()`.
} aml_object_flags_t;

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
typedef uint64_t (*aml_method_implementation_t)(aml_object_t* method, aml_term_arg_list_t* args, aml_object_t* out);

/**
 * @brief ACPI object.
 * @struct aml_object_t
 *
 * A object can represent mode then just a object in the ACPI namespace tree, in practice, its everything.
 * It simply represents any readable or writable entity, this includes the result of operations.
 */
typedef struct aml_object
{
    list_entry_t entry;
    aml_data_type_t type;
    aml_object_flags_t flags;
    list_t children;
    struct aml_object* parent;
    char segment[AML_NAME_LENGTH + 1];
    bool isAllocated;
    union {
        struct
        {
            /**
             * Array of objects of type AML_DATA_BUFFER_FIELD, one for each byte in the buffer, really only used for
             * IndexOp since it needs to return a reference to a BufferField.
             */
            struct aml_object* byteFields;
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
            struct aml_object* indexObject; //!< Used for IndexField.
            struct aml_object* dataObject;  //!< Used for IndexField.
            struct aml_object* opregion;    //!< Used for Field and BankField.
            uint64_t bankValue;             //!< Used for BankField.
            struct aml_object* bank;        //!< Used for BankField.
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
            struct aml_object* target;
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
            struct aml_object** elements;
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
             * Array of objects of type AML_DATA_BUFFER_FIELD, one for each char in the string, really only used for
             * IndexOp since it needs to return a reference to a BufferField.
             */
            struct aml_object* byteFields;
            char* content;
            uint64_t length;
        } string;
        /**
         * @brief Used for forward references.
         */
        struct
        {
            aml_name_string_t nameString;
            aml_object_t* start;
            aml_patch_up_resolve_callback_t callback;
        } unresolved;
        /**
         * @brief Used to implement DefAlias.
         */
        struct
        {
            struct aml_object* target;
        } alias;
    };
    sysfs_dir_t dir;
} aml_object_t;

/**
 * @brief Create a new ACPI object without allocating memory for it.
 *
 * Intended to be used for objects that dont appear in the namespace tree. Use `aml_object_new()` to create objects that
 * should appear in the namespace tree.
 *
 * When the object is no longer needed, it should be deinitialized using `aml_object_deinit()`. But never freed using
 * `aml_object_free()`.
 *
 * @param objectFlags Flags for the new object.
 * @return The new object.
 */
#define AML_OBJECT_CREATE(objectFlags) \
    (aml_object_t) \
    { \
        .entry = LIST_ENTRY_CREATE, .type = AML_DATA_UNINITALIZED, .flags = objectFlags, .children = LIST_CREATE, \
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
 * @brief Allocate a new ACPI object and add it to the parent's children list if a parent is provided.
 *
 * @param parent Pointer to the parent object, can be `NULL`.
 * @param name Name of the new object, must not be longer then `AML_NAME_LENGTH`.
 * @param flags Flags for the new object.
 * @return On success, a pointer to the new object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_new(aml_object_t* parent, const char* name, aml_object_flags_t flags);

/**
 * @brief Free an ACPI object and all its children.
 *
 * @param object Pointer to the object to free.
 */
void aml_object_free(aml_object_t* object);

/**
 * @brief Add a new object at the location and with the name specified by the NameString.
 *
 * @param start The object to start the search from, or `NULL` to start from the root.
 * @param string The Namestring specifying the location and name of the new object.
 * @param flags Flags for the new object.
 * @return On success, a pointer to the new object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_add(aml_object_t* start, aml_name_string_t* string, aml_object_flags_t flags);

/**
 * @brief Initialize an ACPI object as a buffer with the given content.
 *
 * @param object Pointer to the object to initialize.
 * @param buffer Pointer to the buffer.
 * @param bytesToCopy Number of bytes to copy from `buffer` to the object, the rest will be zeroed.
 * @param length The total length of the buffer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_buffer(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length);

/**
 * @brief Initialize an ACPI object as an empty buffer with the given length.
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the buffer will also be the capacity.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_buffer_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize an ACPI object as a buffer field with the given buffer, bit offset and bit size.
 *
 * @param object Pointer to the object to initialize.
 * @param buffer Pointer to the buffer content.
 * @param bitOffset Bit offset within the buffer.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_buffer_field(aml_object_t* object, uint8_t* buffer, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI object as a device or bus.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_device(aml_object_t* object);

/**
 * @brief Initialize an ACPI object as a field unit of type Field.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region object.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_field_unit_field(aml_object_t* object, aml_object_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI object as a field unit of type IndexField.
 *
 * @param object Pointer to the object to initialize.
 * @param indexObject Pointer to the index object.
 * @param dataObject Pointer to the data object.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_field_unit_index_field(aml_object_t* object, aml_object_t* indexObject,
    aml_object_t* dataObject, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI object as a field unit of type BankField.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region object.
 * @param bank Pointer to the bank object.
 * @param bankValue Value to write to the bank object to select the bank structure.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_field_unit_bank_field(aml_object_t* object, aml_object_t* opregion, aml_object_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize an ACPI object as an integer with the given value and bit width.
 *
 * @param object Pointer to the object to initialize.
 * @param value The integer value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_integer(aml_object_t* object, uint64_t value);

/**
 * @brief Initialize an ACPI object as an integer constant with the given value.
 *
 * @param object Pointer to the object to initialize.
 * @param value The integer constant value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_integer_constant(aml_object_t* object, uint64_t value);

/**
 * @brief Initialize an ACPI object as a method with the given flags and address range.
 *
 * @param object Pointer to the object to initialize.
 * @param flags Flags for the method.
 * @param start Pointer to the start of the method's AML bytecode.
 * @param end Pointer to the end of the method's AML bytecode.
 * @param implementation Pointer to a C function that will execute the method, or `NULL` if the method is a normal
 * AML method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_method(aml_object_t* object, aml_method_flags_t* flags, const uint8_t* start,
    const uint8_t* end, aml_method_implementation_t implementation);

/**
 * @brief Initialize an ACPI object as a mutex with the given synchronization level.
 *
 * @param object Pointer to the object to initialize.
 * @param syncLevel The synchronization level of the mutex (0-15).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_mutex(aml_object_t* object, aml_sync_level_t syncLevel);

/**
 * @brief Initialize an ACPI object as an ObjectReference to the given target object.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the ObjectReference will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_object_reference(aml_object_t* object, aml_object_t* target);

/**
 * @brief Initialize an ACPI object as an operation region with the given space, offset, and length.
 *
 * @param object Pointer to the object to initialize.
 * @param space The address space of the operation region.
 * @param offset The offset within the address space.
 * @param length The length of the operation region.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_operation_region(aml_object_t* object, aml_region_space_t space, uint64_t offset,
    uint32_t length);

/**
 * @brief Initialize an ACPI object as a package with the given number of elements.
 *
 * @param object Pointer to the object to initialize.
 * @param length Number of elements the package will be able to hold.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_package(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize an ACPI object as a processor with the given ProcID, PblkAddr, and PblkLen.
 *
 * @param object Pointer to the object to initialize.
 * @param procId The processor ID.
 * @param pblkAddr The pblk address.
 * @param pblkLen The length of the pblk.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_processor(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen);

/**
 * @brief Initialize an ACPI object as a string with the given value.
 *
 * @param object Pointer to the object to initialize.
 * @param str Pointer to the string.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_string(aml_object_t* object, const char* str);

/**
 * @brief Initialize an ACPI object as an empty string with the given length.
 *
 * The string will be initalized with zero chars fol
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the string will also be the capacity.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_string_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize an ACPI object as an unresolved reference and adds it to the patch-up system.
 *
 * This is used for forward references, where a NameString refers to a object that has not yet been defined.
 *
 * @param object Pointer to the object to initialize.
 * @param nameString The NameString representing the path to the target object.
 * @param start The object to start the search from when resolving the reference.
 * @param callback Will be called when the reference is resolved.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_unresolved(aml_object_t* object, aml_name_string_t* nameString, aml_object_t* start,
    aml_patch_up_resolve_callback_t callback);

/**
 * @brief Initialize an ACPI object as an alias to the given target object.
 *
 * This is used to implement the DefAlias structure.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the alias will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_init_alias(aml_object_t* object, aml_object_t* target);

/**
 * @brief Deinitialize an ACPI object, freeing any resources associated with it and setting its type to
 * `AML_DATA_UNINITALIZED`.
 *
 * @param object Pointer to the object to deinitialize.
 */
void aml_object_deinit(aml_object_t* object);

/**
 * @brief Traverse alias objects to find the target object.
 *
 * Will follow the alias chain until it reaches a non-alias object or a `NULL` pointer.
 *
 * @param object Pointer to the starting object.
 * @return A pointer to the target object, or `NULL` if the input object is `NULL`.
 */
aml_object_t* aml_object_traverse_alias(aml_object_t* object);

/**
 * @brief Find a child object with the given name.
 *
 * @param parent Pointer to the parent object.
 * @param name Name of the child object to find, must be `AML_NAME_LENGTH` chars long.
 * @return On success, a pointer to the found child object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_find_child(aml_object_t* parent, const char* name);

/**
 * @brief Walks the ACPI namespace tree to find the object corresponding to the given path.
 *
 * The path is a null-terminated string with segments separated by dots (e.g., "DEV0.SUB0.METH").
 * A leading backslash indicates an absolute path from the root (e.g., "\DEV0.SUB0.METH").
 * A leading caret indicates a relative path from the start object's parent (e.g., "^SUB0.METH").
 *
 * @param start The object to start the search from, or `NULL` to start from the root.
 * @param path The path string to search for.
 * @return On success, a pointer to the found object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_find(aml_object_t* start, const char* path);

/**
 * @brief Store bits into a object at the specified bit offset and size.
 *
 * This function only works on the following object types:
 * - Integers
 * - Buffers
 * - Strings
 *
 * @param object Pointer to the object to store bits into.
 * @param value The bits to store (only the least significant `bitSize` bits are used).
 * @param bitOffset The bit offset within the object's data to start storing to.
 * @param bitSize The number of bits to store (up to 64 or the object's bit width).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_put_bits_at(aml_object_t* object, uint64_t value, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Retrieve bits from a object at the specified bit offset and size.
 *
 * This function only works on the following object types:
 * - Integers
 * - Buffers
 * - Strings
 *
 * @param object Pointer to the object to extract bits from.
 * @param bitOffset The bit offset within the object's data to start extracting from.
 * @param bitSize The number of bits to extract (up to 64 or the object's bit width).
 * @param out Pointer to a buffer where the extracted bits will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_get_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint64_t* out);

/** @} */
