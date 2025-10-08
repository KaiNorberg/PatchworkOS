#pragma once

#include "aml_patch_up.h"
#include "encoding/data.h"
#include "encoding/name.h"
#include "encoding/named.h"
#include "fs/sysfs.h"
#include "runtime/mutex.h"
#include "utils/ref.h"

#include <stdint.h>

typedef struct aml_term_arg_list aml_term_arg_list_t;

typedef struct aml_object aml_object_t;
typedef struct aml_opregion aml_opregion_t;
typedef struct aml_string aml_string_t;
typedef struct aml_method aml_method_t;

/**
 * @brief Object
 * @defgroup kernel_acpi_aml_object Object
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Maximum length of an ACPI name.
 */
#define AML_NAME_LENGTH 4

/**
 * @brief ACPI data types.
 * @enum aml_type_t
 *
 * Note that objects can obviously only have one type but we use bitflags here to make it easier to define groups of
 * types.
 *
 * @see Section 19.3.5 table 19.5 of the ACPI specification for more details.
 */
typedef enum
{
    AML_UNINITIALIZED = 0,
    AML_BUFFER = 1 << 0,
    AML_BUFFER_FIELD = 1 << 1,
    AML_DEBUG_OBJECT = 1 << 2,
    AML_DEVICE = 1 << 3,
    AML_EVENT = 1 << 4,
    AML_FIELD_UNIT = 1 << 5,
    AML_INTEGER = 1 << 6,
    AML_INTEGER_CONSTANT = 1 << 7,
    AML_METHOD = 1 << 8,
    AML_MUTEX = 1 << 9,
    AML_OBJECT_REFERENCE = 1 << 10,
    AML_OPERATION_REGION = 1 << 11,
    AML_PACKAGE = 1 << 12,
    AML_POWER_RESOURCE = 1 << 13,
    AML_PROCESSOR = 1 << 14,
    AML_RAW_DATA_BUFFER = 1 << 15,
    AML_STRING = 1 << 16,
    AML_THERMAL_ZONE = 1 << 17,
    AML_ALIAS = 1 << 18,      ///< Not in the spec, used internally to represent Aliases.
    AML_UNRESOLVED = 1 << 19, ///< Not in the spec, used internally to represent unresolved references.
    /**
     * All data types that can be retrieved from a DataObject (section 20.2.3).
     *
     * You could also define it as static data, as in not stored in some firmware register or similar.
     */
    AML_DATA_OBJECTS = AML_INTEGER | AML_STRING | AML_BUFFER | AML_PACKAGE,
    /**
     * All data types that can be retrived from a DataRefObject (section 20.2.3).
     */
    AML_DATA_REF_OBJECTS = AML_DATA_OBJECTS | AML_OBJECT_REFERENCE,
    /**
     * All data types that can contain named objects, packages contain unnamed objects only and are excluded.
     */
    AML_CONTAINERS = AML_DEVICE | AML_PROCESSOR | AML_METHOD | AML_THERMAL_ZONE | AML_POWER_RESOURCE,
    /**
     * All data types.
     */
    AML_ALL_TYPES = AML_BUFFER | AML_BUFFER_FIELD | AML_DEBUG_OBJECT | AML_DEVICE | AML_EVENT | AML_FIELD_UNIT |
        AML_INTEGER | AML_METHOD | AML_MUTEX | AML_OBJECT_REFERENCE | AML_OPERATION_REGION | AML_PACKAGE |
        AML_POWER_RESOURCE | AML_PROCESSOR | AML_RAW_DATA_BUFFER | AML_STRING | AML_THERMAL_ZONE,
    AML_TYPE_AMOUNT = 20, ///< Not a type, just the amount of types.
} aml_type_t;

/**
 * @brief Flags for ACPI objects.
 * @enum aml_object_flags_t
 */
typedef enum
{
    AML_OBJECT_NONE = 0,            ///< No flags.
    AML_OBJECT_ROOT = 1 << 0,       ///< Is the root object.
    AML_OBJECT_PREDEFINED = 1 << 1, ///< Is a predefined object.
    AML_OBJECT_LOCAL = 1 << 2,      ///< Is a local variable.
    AML_OBJECT_ARG = 1 << 3,        ///< Is a method argument.
    AML_OBJECT_NAMED = 1 << 4,      ///< The object appears in the namespace tree. Will be set in `aml_object_add()`.
    AML_OBJECT_ELEMENT = 1 << 5,    ///< Is an element of a package.
} aml_object_flags_t;

/**
 * @brief Field Unit types.
 * @enum aml_field_unit_type_t
 *
 * Since the ACPI spec does not differentiate between "objects" of type Field, IndexField and BankField, instead just
 * calling them all FieldUnits, we use this enum to differentiate between different FieldUnit types, even if it might
 * be cleaner to use aml_type_t for this.
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
typedef uint64_t (
    *aml_method_implementation_t)(aml_method_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* out);

/**
 * @brief Defines the location of an object in the ACPI namespace.
 * @struct aml_name_t
 */
typedef struct aml_name
{
    list_entry_t entry;                ///< Used to store the object in a its parent's named object list.
    aml_object_t* parent;              ///< Pointer to the parent object, can be `NULL`.
    char segment[AML_NAME_LENGTH + 1]; ///< The name of the object.
    sysfs_dir_t dir;                   ///< Used to expose the object in the filesystem.
} aml_name_t;

/**
 * @brief Common header for all AML objects.
 */
#define AML_OBJECT_COMMON_HEADER \
    ref_t ref; \
    list_entry_t stateEntry; \
    aml_object_flags_t flags; \
    aml_name_t name; \
    aml_type_t type; \
    aml_state_t* state

/**
 * @brief Data for a buffer object.
 * @struct aml_buffer_t
 */
typedef struct aml_buffer
{
    AML_OBJECT_COMMON_HEADER;
    uint8_t* content;
    uint64_t length;
} aml_buffer_t;

/**
 * @brief Data for a buffer field object.
 * @struct aml_buffer_field_t
 */
typedef struct aml_buffer_field
{
    AML_OBJECT_COMMON_HEADER;
    bool isString;        ///< True if the buffer field was created from a string.
    aml_buffer_t* buffer; ///< Used if the buffer field is created from a buffer.
    aml_string_t* string; ///< Used if the buffer field is created from a string.
    aml_bit_size_t bitOffset;
    aml_bit_size_t bitSize;
} aml_buffer_field_t;

/**
 * @brief Data for a device object.
 * @struct aml_device_t
 */
typedef struct aml_device
{
    AML_OBJECT_COMMON_HEADER;
    list_t namedObjects;
} aml_device_t;

/**
 * @brief Data placeholder for an event object.
 * @struct aml_event_t
 *
 * TODO: Implement event object functionality.
 */
typedef struct aml_event
{
    AML_OBJECT_COMMON_HEADER;
} aml_event_t;

/**
 * @brief Data for a field unit object.
 * @struct aml_field_unit_t
 */
typedef struct aml_field_unit
{
    AML_OBJECT_COMMON_HEADER;
    aml_field_unit_type_t fieldType; ///< The type of field unit.
    aml_field_unit_t* index;         ///< Used for IndexField.
    aml_field_unit_t* data;          ///< Used for IndexField.
    aml_object_t* bankValue;         ///< Used for BankField.
    aml_field_unit_t* bank;          ///< Used for BankField.
    aml_opregion_t* opregion;        ///< Used for Field and BankField.
    aml_field_flags_t fieldFlags;    ///< Used for Field, IndexField and BankField.
    aml_bit_size_t bitOffset;        ///< Used for Field, IndexField and BankField.
    aml_bit_size_t bitSize;          ///< Used for Field, IndexField and BankField.
} aml_field_unit_t;

/**
 * @brief Data for an integer object.
 * @struct aml_integer_t
 */
typedef struct aml_integer
{
    AML_OBJECT_COMMON_HEADER;
    uint64_t value;
} aml_integer_t;

/**
 * @brief Data for an integer constant object.
 * @struct aml_integer_constant_t
 */
typedef struct aml_integer_constant
{
    AML_OBJECT_COMMON_HEADER;
    uint64_t value;
} aml_integer_constant_t;

/**
 * @brief Data for a method object.
 * @struct aml_method_t
 */
typedef struct aml_method
{
    AML_OBJECT_COMMON_HEADER;
    /**
     * Pointer to the C function that will execute the method. Really just used to implement predefined the
     * predefined method _OSI. If `implementation` is `NULL`, the method is just a normal AML method.
     */
    aml_method_implementation_t implementation;
    aml_method_flags_t methodFlags;
    const uint8_t* start;
    const uint8_t* end;
    list_t namedObjects;
    aml_mutex_id_t mutex;
} aml_method_t;

/**
 * @brief Data for a mutex object.
 * @struct aml_mutex_t
 */
typedef struct aml_mutex
{
    AML_OBJECT_COMMON_HEADER;
    aml_sync_level_t syncLevel;
    aml_mutex_id_t mutex;
} aml_mutex_t;

/**
 * @brief Data for an object reference object.
 * @struct aml_object_reference_t
 */
typedef struct aml_object_reference
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* target;
} aml_object_reference_t;

/**
 * @brief Data for an operation region object.
 * @struct aml_opregion_t
 */
typedef struct aml_opregion
{
    AML_OBJECT_COMMON_HEADER;
    aml_region_space_t space;
    uint64_t offset;
    uint32_t length;
} aml_opregion_t;

/**
 * @brief Data for a package object.
 * @struct aml_package_t
 */
typedef struct aml_package
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t** elements;
    uint64_t length;
} aml_package_t;

typedef struct aml_power_resource
{
    AML_OBJECT_COMMON_HEADER;
    aml_system_level_t systemLevel;
    aml_resource_order_t resourceOrder;
    list_t namedObjects;
} aml_power_resource_t;

/**
 * @brief Data for a processor object.
 * @struct aml_processor_t
 */
typedef struct aml_processor
{
    AML_OBJECT_COMMON_HEADER;
    aml_proc_id_t procId;
    aml_pblk_addr_t pblkAddr;
    aml_pblk_len_t pblkLen;
    list_t namedObjects;
} aml_processor_t;

/**
 * @brief Data for a string object.
 * @struct aml_string_t
 */
typedef struct aml_string
{
    AML_OBJECT_COMMON_HEADER;
    char* content;
    uint64_t length;
} aml_string_t;

/**
 * @brief Data for a thermal zone object.
 * @struct aml_thermal_zone_t
 */
typedef struct aml_thermal_zone
{
    AML_OBJECT_COMMON_HEADER;
    list_t namedObjects;
} aml_thermal_zone_t;

/**
 * @brief Data for an alias object.
 * @struct aml_alias_t
 */
typedef struct aml_alias
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* target;
} aml_alias_t;

/**
 * @brief Data for an unresolved object.
 * @struct aml_unresolved_t
 */
typedef struct aml_unresolved
{
    AML_OBJECT_COMMON_HEADER;
    aml_name_string_t nameString;             ///< The NameString representing the path to the target object.
    aml_object_t* from;                       ///< The object to start the search from when resolving the reference.
    aml_patch_up_resolve_callback_t callback; ///< The callback to call when a matching object is found.
} aml_unresolved_t;

/**
 * @brief ACPI object.
 * @struct aml_object_t
 */
typedef struct aml_object
{
    union {
        struct
        {
            AML_OBJECT_COMMON_HEADER;
        };
        aml_buffer_t buffer;
        aml_buffer_field_t bufferField;
        aml_device_t device;
        aml_event_t event;
        aml_field_unit_t fieldUnit;
        aml_integer_t integer;
        aml_integer_constant_t integerConstant;
        aml_method_t method;
        aml_mutex_t mutex;
        aml_object_reference_t objectReference;
        aml_opregion_t opregion;
        aml_package_t package;
        aml_power_resource_t powerResource;
        aml_processor_t processor;
        aml_string_t string;
        aml_thermal_zone_t thermalZone;
        aml_alias_t alias;
        aml_unresolved_t unresolved;
    };
} aml_object_t;

/**
 * @brief Macro to get the name of an ACPI object.
 *
 * @param obj Pointer to the object.
 * @return The name of the object, or "<unnamed>" if the object is not named.
 */
#define AML_OBJECT_GET_NAME(obj) ((obj)->flags & AML_OBJECT_NAMED ? (obj)->name.segment : "<unnamed>")

/**
 * @brief Get the total amount of allocated ACPI objects.
 *
 * @return The total amount of allocated ACPI objects.
 */
uint64_t aml_object_get_total_count(void);

/**
 * @brief Allocate a new ACPI object.
 *
 * There is no `aml_object_free()` instead always use `DEREF()` to free an object, since objects are reference counted.
 *
 * You could also use `DEREF_DEFER()` to dereference the object when the current scope ends.
 *
 * @param state Pointer to the AML state the object will belong to, can be `NULL`.
 * @param flags Flags for the new object, `AML_OBJECT_NAMED` is not allowed here.
 * @return On success, a pointer to the new object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_new(aml_state_t* state, aml_object_flags_t flags);

/**
 * @brief Deinitialize an ACPI object, setting its type to `AML_UNINITIALIZED`.
 *
 * @param object Pointer to the object to deinitialize.
 */
void aml_object_deinit(aml_object_t* object);

/**
 * @brief Recursively count how many children an object has.
 *
 * This will also count package elements, any cached byteFields, etc. All objects that are owned by the parent
 * object will be counted.
 *
 * @param parent Pointer to the parent object.
 * @return The total amount of children the object has.
 */
uint64_t aml_object_count_children(aml_object_t* parent);

/**
 * @brief Add a child object to a parent object with the given name.
 *
 * Will set the `AML_OBJECT_NAMED` flag in `child->flags` and initialize `child->named`.
 *
 * Creates a new reference to `child`, so you should `DEREF()` it after a successful call to this function.
 *
 * @param parent Pointer to the parent object.
 * @param child Pointer to the child object to add.
 * @param name Name of the child object to add, must be exactly `AML_NAME_LENGTH` chars long, does not need to be
 * null-terminated.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_add_child(aml_object_t* parent, aml_object_t* child, const char* name);

/**
 * @brief Make a object named by adding it to the ACPI namespace tree at the location specified by the namestring.
 *
 * Will set the `AML_OBJECT_NAMED` flag in `object->flags` and initialize `object->named`.
 *
 * Creates a new reference to `object`, so you should `DEREF()` it after a successful call to this function.
 *
 * @param object Pointer to the object to add.
 * @param from Pointer to the object to start the search from, can be `NULL` to start from the root.
 * @param nameString Pointer to the name string, can be `NULL` if `object->flags & AML_OBJECT_ROOT`, must have atleast
 * one name segment otherwise.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_add(aml_object_t* object, aml_object_t* from, const aml_name_string_t* nameString);

/**
 * @brief Remove a object from the ACPI namespace tree.
 *
 * Will clear the `AML_OBJECT_NAMED` flag in `object->flags` and deinitialize `object->named`.
 *
 * Does nothing if the object is not named.
 *
 * @param object Pointer to the object to remove.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_remove(aml_object_t* object);

/**
 * @brief Find a child object with the given name.
 *
 * @param parent Pointer to the parent object.
 * @param name Name of the child object to find, must be `AML_NAME_LENGTH` chars long.
 * @return On success, a reference to the found child object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_find_child(aml_object_t* parent, const char* name);

/**
 * @brief Walks the ACPI namespace tree to find the object corresponding to the given path.
 *
 * The path is a null-terminated string with segments separated by dots (e.g., "DEV0.SUB0.METH").
 * A leading backslash indicates an absolute path from the root (e.g., "\DEV0.SUB0.METH").
 * A leading caret indicates a relative path from the start object's parent (e.g., "^SUB0.METH").
 *
 * @see aml_name_string_resolve() for more details on path resolution.
 *
 * @param start The object to start the search from, or `NULL` to start from the root.
 * @param path The path string to search for.
 * @return On success, a reference to the found object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_find(aml_object_t* start, const char* path);

/**
 * @brief Store bits into a object at the specified bit offset and size.
 *
 * Only supports Integers and Buffers.
 *
 * @param object Pointer to the object to store bits into.
 * @param value The bits to store (only the least significant `bitSize` bits are used).
 * @param bitOffset The bit offset within the object's data to start storing to.
 * @param bitSize The number of bits to extract, up to 64.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_put_bits_at(aml_object_t* object, uint64_t value, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Retrieve bits from a object at the specified bit offset and size.
 *
 * Only supports Integers, IntegerConstants and Buffers.
 *
 * If a out of bounds access is attempted, the bits that are out of bounds will be read as zero.
 *
 * @param object Pointer to the object to extract bits from.
 * @param bitOffset The bit offset within the object's data to start extracting from.
 * @param bitSize The number of bits to extract, up to 64.
 * @param out Pointer to a buffer where the extracted bits will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_get_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint64_t* out);

/**
 * @brief Initialize a object as a buffer with the given content.
 *
 * @param object Pointer to the object to initialize.
 * @param buffer Pointer to the buffer.
 * @param bytesToCopy Number of bytes to copy from `buffer` to the object, the rest will be zeroed.
 * @param length The total length of the buffer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_init(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length);

/**
 * @brief Initialize a object as an empty buffer with the given length.
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the buffer will also be the capacity.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_init_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize a object as a buffer field with the given buffer, bit offset and bit size.
 *
 * @param object Pointer to the object to initialize.
 * @param buffer Pointer to the buffer.
 * @param bitOffset Bit offset within the buffer.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_field_init_buffer(aml_object_t* object, aml_buffer_t* buffer, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Initialize a object as a buffer field with the given string, bit offset and bit size.
 *
 * @param object Pointer to the object to initialize.
 * @param string Pointer to the string.
 * @param bitOffset Bit offset within the string.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_field_init_string(aml_object_t* object, aml_string_t* string, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Initialize a object as a debug object.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_debug_object_init(aml_object_t* object);

/**
 * @brief Initialize a object as a device or bus.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_device_init(aml_object_t* object);

/**
 * @brief Initialize a object as an event.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_event_init(aml_object_t* object);

/**
 * @brief Initialize a object as a field unit of type Field.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_field_init(aml_object_t* object, aml_opregion_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize a object as a field unit of type IndexField.
 *
 * @param object Pointer to the object to initialize.
 * @param index Pointer to the index field.
 * @param data Pointer to the data field.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_index_field_init(aml_object_t* object, aml_field_unit_t* index, aml_field_unit_t* data,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize a object as a field unit of type BankField.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region.
 * @param bank Pointer to the bank field.
 * @param bankValue Value to write to the bank object to select the bank structure.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_bank_field_init(aml_object_t* object, aml_opregion_t* opregion, aml_field_unit_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Initialize a object as an integer with the given value and bit width.
 *
 * @param object Pointer to the object to initialize.
 * @param value The integer value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_integer_init(aml_object_t* object, uint64_t value);

/**
 * @brief Initialize a object as an integer constant with the given value.
 *
 * @param object Pointer to the object to initialize.
 * @param value The integer constant value to set.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_integer_constant_init(aml_object_t* object, uint64_t value);

/**
 * @brief Initialize a object as a method with the given flags and address range.
 *
 * @param object Pointer to the object to initialize.
 * @param flags Flags for the method.
 * @param start Pointer to the start of the method's AML bytecode.
 * @param end Pointer to the end of the method's AML bytecode.
 * @param implementation Pointer to a C function that will execute the method, or `NULL` if the method is a normal
 * AML method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_init(aml_object_t* object, aml_method_flags_t flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation);

/**
 * @brief Initialize a object as a mutex with the given synchronization level.
 *
 * @param object Pointer to the object to initialize.
 * @param syncLevel The synchronization level of the mutex (0-15).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_init(aml_object_t* object, aml_sync_level_t syncLevel);

/**
 * @brief Initialize a object as an ObjectReference to the given target object.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the ObjectReference will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_reference_init(aml_object_t* object, aml_object_t* target);

/**
 * @brief Initialize a object as an operation region with the given space, offset, and length.
 *
 * @param object Pointer to the object to initialize.
 * @param space The address space of the operation region.
 * @param offset The offset within the address space.
 * @param length The length of the operation region.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_operation_region_init(aml_object_t* object, aml_region_space_t space, uint64_t offset, uint32_t length);

/**
 * @brief Initialize a object as a package with the given number of elements.
 *
 * @param object Pointer to the object to initialize.
 * @param length Number of elements the package will be able to hold.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_init(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize a object as a power resource with the given system level and resource order.
 *
 * @param object Pointer to the object to initialize.
 * @param systemLevel The system level of the power resource.
 * @param resourceOrder The resource order of the power resource.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_power_resource_init(aml_object_t* object, aml_system_level_t systemLevel,
    aml_resource_order_t resourceOrder);

/**
 * @brief Initialize a object as a processor with the given ProcID, PblkAddr, and PblkLen.
 *
 * @param object Pointer to the object to initialize.
 * @param procId The processor ID.
 * @param pblkAddr The pblk address.
 * @param pblkLen The length of the pblk.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_processor_init(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen);

/**
 * @brief Initialize a object as an empty string with the given length.
 *
 * The string will be initalized with zero chars and be null terminated.
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the string, not including the null terminator.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_init_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Initialize a object as a string with the given value.
 *
 * @param object Pointer to the object to initialize.
 * @param str Pointer to the string.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_init(aml_object_t* object, const char* str);

/**
 * @brief Initialize a object as a thermal zone.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_thermal_zone_init(aml_object_t* object);

/**
 * @brief Initialize a object as an alias to the given target object.
 *
 * This is used to implement the DefAlias structure.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the alias will point to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_alias_init(aml_object_t* object, aml_object_t* target);

/**
 * @brief Traverse an alias object to get the target object.
 *
 * If the target is also an alias, it will be traversed recursively until a non-alias object is found.
 *
 * @param alias Pointer to the alias object to traverse.
 * @return On success, a reference to the target object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_alias_traverse(aml_alias_t* alias);

/**
 * @brief Initialize a object as an unresolved reference with the given namestring and starting point.
 *
 * The object will be resolved later by calling `aml_patch_up_resolve_all()`.
 *
 * @param object Pointer to the object to initialize.
 * @param nameString Pointer to the namestring representing the path to the target object.
 * @param from Pointer to the object to start the search from, can be `NULL` to start from the root.
 * @param callback Pointer to a callback function that will be called when a matching object is found
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_unresolved_init(aml_object_t* object, const aml_name_string_t* nameString, aml_object_t* from,
    aml_patch_up_resolve_callback_t callback);

/** @} */
