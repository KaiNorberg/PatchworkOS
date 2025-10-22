#pragma once

#include "encoding/name.h"
#include "fs/sysfs.h"
#include "integer.h"
#include "namespace.h"
#include "patch_up.h"
#include "runtime/mutex.h"
#include "utils/ref.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;
typedef struct aml_object aml_object_t;
typedef struct aml_opregion_obj aml_opregion_obj_t;
typedef struct aml_string_obj aml_string_obj_t;
typedef struct aml_method_obj aml_method_obj_t;

/**
 * @brief Object
 * @defgroup kernel_acpi_aml_object Object
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Size of buffers used for small objects optimization.
 */
#define AML_SMALL_BUFFER_SIZE 32

/**
 * @brief Size of string buffers used for small objects optimization, not including the null terminator.
 */
#define AML_SMALL_STRING_SIZE AML_SMALL_BUFFER_SIZE

/**
 * @brief Size of package element arrays used for small objects optimization.
 */
#define AML_SMALL_PACKAGE_SIZE 4

/**
 * @brief Amount of objects to store in the cache before freeing them instead.
 */
#define AML_OBJECT_CACHE_SIZE 64

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
    /**
     * The spec does defined a separate Integer Constant type, but the spec seems very inconsistent about how to
     * actually use it or even what it is. In 19.3.5 its "Created by the ASL terms 'Zero', 'One', 'Ones', and
     * 'Revision'.". But in 19.6.102 the package creation example referes to a normal number "0x3400" as an Integer
     * Constant. And there are also unanswered questions about what happens if a named object is created as an Integer
     * Constant. The ACPICA tests seem to just treat even the result of 'Zero/One/Ones' as a normal Integer. I could go
     * on. So unless ive missed something obvious, we just pretend it doesn't exist and treat it as a normal Integer.
     */
    // AML_INTEGER_CONSTANT = 1 << 7,
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
    AML_ALIAS = 1 << 18,            ///< Not in the spec, used internally to represent Aliases.
    AML_UNRESOLVED = 1 << 19,       ///< Not in the spec, used internally to represent unresolved references.
    AML_PREDEFINED_SCOPE = 1 << 20, ///< Not in the spec, used internally to represent \_SB, \_GPE, etc.
    AML_ARG = 1 << 21,              ///< Not in the spec, used internally to represent method arguments.
    AML_LOCAL = 1 << 22,            ///< Not in the spec, used internally to represent method local variables.
    /**
     * All data types that can be retrieved from a ComputationalData object (section 20.2.3).
     */
    AML_COMPUTATIONAL_DATA_OBJECTS = AML_INTEGER | AML_STRING | AML_BUFFER,
    /**
     * All data types that can be retrieved from a DataObject (section 20.2.3).
     *
     * You could also define it as static data, as in not stored in some firmware register or similar.
     */
    AML_DATA_OBJECTS = AML_COMPUTATIONAL_DATA_OBJECTS | AML_PACKAGE,
    /**
     * All data types that can be retrived from a DataRefObject (section 20.2.3).
     */
    AML_DATA_REF_OBJECTS = AML_DATA_OBJECTS | AML_OBJECT_REFERENCE,
    /**
     * All data types that can contain named objects, packages contain unnamed objects only and are excluded.
     */
    AML_NAMESPACES =
        AML_DEVICE | AML_PROCESSOR | AML_METHOD | AML_THERMAL_ZONE | AML_POWER_RESOURCE | AML_PREDEFINED_SCOPE,
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
    AML_OBJECT_NONE = 0,       ///< No flags.
    AML_OBJECT_ROOT = 1 << 0,  ///< Is the root object.
    AML_OBJECT_NAMED = 1 << 1, ///< Appears in the namespace tree. Will be set in `aml_object_add()`.
    /**
     * The first time this object is used an exception will be raised. This is used such that when a method fails to
     * implicitly or explicitly return a value the "synthetic" return value will raise an exception when used.
     *
     * Any copy of an object with this flag will also have this flag set.
     */
    AML_OBJECT_EXCEPTION_ON_USE = 1 << 2,
    /**
     * The object is exposed in sysfs. Will be set in `aml_namespace_expose()`.
     */
    AML_OBJECT_EXPOSED_IN_SYSFS = 1 << 3,
} aml_object_flags_t;

/**
 * @brief Object id type.
 * @typedef aml_object_id_t
 *
 * Used in a namespace in combination with a childs name to generate a hash to locate the child in the namespace.
 */
typedef uint64_t aml_object_id_t;

/**
 * @brief Value for an invalid object id.
 */
#define AML_OBJECT_ID_NONE 0

/**
 * @brief Field Unit types.
 * @enum aml_field_unit_obj_type_t
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
} aml_field_unit_obj_type_t;

/**
 * @brief Method Implementation function type.
 * @typedef aml_method_implementation_t
 */
typedef aml_object_t* (*aml_method_implementation_t)(aml_method_obj_t* method, aml_object_t** args, uint64_t argCount);

/**
 * @brief Common header for all AML objects.
 *
 * Members:
 * - `ref` Reference count for the object.
 * - `id` The unique id of the object.
 * - `name` The name of the object.
 * - `mapEntry` Entry for the namespace `map` member.
 * - `listEntry` Entry for the namespace `objects` member or the object cache list.
 * - `overlay` The overlay this object is part of, `NULL` if part of the global namespace or unanamed.
 * - `children` List of children, children hold references to the parent, parent does not hold references to children.
 * - `siblingsEntry` Entry for the parent's `children` member.
 * - `parent` Pointer to the parent object, `NULL` if root or unnamed.
 * - `flags` Flags for the object, see `aml_object_flags_t` for more details.
 * - `type` The type of the object, see `aml_type_t` for more details.
 * - `dir` Sysfs directory for the object, only valid if `flags` has `AML_OBJECT_EXPOSED_IN_SYSFS` set.
 */
#define AML_OBJECT_COMMON_HEADER \
    ref_t ref; \
    aml_object_id_t id; \
    aml_name_t name; \
    map_entry_t mapEntry; \
    list_entry_t listEntry; \
    aml_namespace_overlay_t* overlay; \
    list_t children; \
    list_entry_t siblingsEntry; \
    aml_object_t* parent; \
    aml_object_flags_t flags; \
    aml_type_t type; \
    dentry_t* dir

/**
 * @brief Data for a buffer object.
 * @struct aml_buffer_obj_t
 */
typedef struct aml_buffer_obj
{
    AML_OBJECT_COMMON_HEADER;
    uint8_t* content;
    uint64_t length;
    uint8_t smallBuffer[AML_SMALL_BUFFER_SIZE]; ///< Used for small object optimization.
} aml_buffer_obj_t;

/**
 * @brief Data for a buffer field object.
 * @struct aml_buffer_field_obj_t
 */
typedef struct aml_buffer_field_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* target;
    aml_bit_size_t bitOffset;
    aml_bit_size_t bitSize;
} aml_buffer_field_obj_t;

/**
 * @brief Data placeholder for an event object.
 * @struct aml_event_obj_t
 *
 * TODO: Implement event object functionality.
 */
typedef struct aml_event_obj
{
    AML_OBJECT_COMMON_HEADER;
} aml_event_obj_t;

/**
 * @brief Data for a field unit object.
 * @struct aml_field_unit_obj_t
 */
typedef struct aml_field_unit_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_field_unit_obj_type_t fieldType; ///< The type of field unit.
    aml_field_unit_obj_t* index;         ///< Used for IndexField.
    aml_field_unit_obj_t* data;          ///< Used for IndexField.
    aml_object_t* bankValue;             ///< Used for BankField.
    aml_field_unit_obj_t* bank;          ///< Used for BankField.
    aml_opregion_obj_t* opregion;        ///< Used for Field and BankField.
    aml_field_flags_t fieldFlags;        ///< Used for Field, IndexField and BankField.
    aml_bit_size_t bitOffset;            ///< Used for Field, IndexField and BankField.
    aml_bit_size_t bitSize;              ///< Used for Field, IndexField and BankField.
} aml_field_unit_obj_t;

/**
 * @brief Data for an integer object.
 * @struct aml_integer_obj_t
 */
typedef struct aml_integer_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_integer_t value;
} aml_integer_obj_t;

/**
 * @brief Data for an integer constant object.
 * @struct aml_integer_constant_obj_t
 */
typedef struct aml_integer_constant_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_integer_t value;
} aml_integer_constant_obj_t;

/**
 * @brief Data for a method object.
 * @struct aml_method_obj_t
 */
typedef struct aml_method_obj
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
    aml_mutex_id_t mutex;
} aml_method_obj_t;

/**
 * @brief Data for a mutex object.
 * @struct aml_mutex_obj_t
 */
typedef struct aml_mutex_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_sync_level_t syncLevel;
    aml_mutex_id_t mutex;
} aml_mutex_obj_t;

/**
 * @brief Data for an object reference object.
 * @struct aml_object_reference_obj_t
 */
typedef struct aml_object_referencev
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* target;
} aml_object_reference_obj_t;

/**
 * @brief Data for an operation region object.
 * @struct aml_opregion_obj_t
 */
typedef struct aml_opregion_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_region_space_t space;
    uintptr_t offset;
    uint32_t length;
} aml_opregion_obj_t;

/**
 * @brief Data for a package object.
 * @struct aml_package_obj_t
 *
 * Packages use an array to store the elements not a linked list since indexing is very common with packages.
 */
typedef struct aml_package_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t** elements;
    uint64_t length;
    aml_object_t* smallElements[AML_SMALL_PACKAGE_SIZE]; ///< Used for small object optimization.
} aml_package_obj_t;

typedef struct aml_power_resource_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_system_level_t systemLevel;
    aml_resource_order_t resourceOrder;
} aml_power_resource_obj_t;

/**
 * @brief Data for a processor object.
 * @struct aml_processor_obj_t
 */
typedef struct aml_processor_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_proc_id_t procId;
    aml_pblk_addr_t pblkAddr;
    aml_pblk_len_t pblkLen;
} aml_processor_obj_t;

/**
 * @brief Data for a string object.
 * @struct aml_string_obj_t
 */
typedef struct aml_string_obj
{
    AML_OBJECT_COMMON_HEADER;
    char* content;
    uint64_t length;
    char smallString[AML_SMALL_STRING_SIZE + 1]; ///< Used for small object optimization.
} aml_string_obj_t;

/**
 * @brief Data for an alias object.
 * @struct aml_alias_obj_t
 */
typedef struct aml_alias_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* target;
} aml_alias_obj_t;

/**
 * @brief Data for an unresolved object.
 * @struct aml_unresolved_obj_t
 */
typedef struct aml_unresolved_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_name_string_t nameString;             ///< The NameString representing the path to the target object.
    aml_object_t* from;                       ///< The object to start the search from when resolving the reference.
    aml_patch_up_resolve_callback_t callback; ///< The callback to call when a matching object is found.
} aml_unresolved_obj_t;

/**
 * @brief Data for an argument object.
 * @struct aml_arg_obj_t
 *
 * Arguments are disgusting but the way passing arguments work is described in section 5.5.2.3 of the ACPI
 * specification.
 */
typedef struct aml_arg_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* value; ///< The object that was passed as the argument.
} aml_arg_obj_t;

/**
 * @brief Data for a local variable object.
 * @struct aml_local_obj_t
 */
typedef struct aml_local_obj
{
    AML_OBJECT_COMMON_HEADER;
    aml_object_t* value; ///< The value of the local variable.
} aml_local_obj_t;

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
        aml_buffer_obj_t buffer;
        aml_buffer_field_obj_t bufferField;
        aml_event_obj_t event;
        aml_field_unit_obj_t fieldUnit;
        aml_integer_obj_t integer;
        aml_integer_constant_obj_t integerConstant;
        aml_method_obj_t method;
        aml_mutex_obj_t mutex;
        aml_object_reference_obj_t objectReference;
        aml_opregion_obj_t opregion;
        aml_package_obj_t package;
        aml_power_resource_obj_t powerResource;
        aml_processor_obj_t processor;
        aml_string_obj_t string;

        aml_alias_obj_t alias;
        aml_unresolved_obj_t unresolved;
        aml_arg_obj_t arg;
        aml_local_obj_t local;
    };
} aml_object_t;

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
 * @return On success, a pointer to the new object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_object_new(void);

/**
 * @brief Clear the data of a object, setting its type to `AML_UNINITIALIZED`.
 *
 * @param object Pointer to the object to clear.
 */
void aml_object_clear(aml_object_t* object);

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
 * @brief Store bits into a object at the specified bit offset and size.
 *
 * Only supports Integers, Strings and Buffers.
 *
 * If a out of bounds access is attempted, the bits that are out of bounds will be ignored.
 *
 * All objects, Intergers, Strings and Buffers are writen to as if they were little-endian Integers.
 *
 * @param object Pointer to the object to store bits into.
 * @param bitOffset The bit offset within the object's data to start storing to.
 * @param bitSize The number of bits to store, `in` must be large enough to hold this many bits.
 * @param in Pointer to a buffer containing the bits to store.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_set_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint8_t* in);

/**
 * @brief Retrieve bits from a object at the specified bit offset and size.
 *
 * Only supports Integers, Strings and Buffers.
 *
 * If a out of bounds access is attempted, the bits that are out of bounds will be read as zero.
 *
 * All objects, Intergers, Strings and Buffers are read from as if they were little-endian Integers.
 *
 * @param object Pointer to the object to extract bits from.
 * @param bitOffset The bit offset within the object's data to start extracting from.
 * @param bitSize The number of bits to store, `out` must be large enough to hold this many bits.
 * @param out Pointer to a buffer where the extracted bits will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_get_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint8_t* out);

/**
 * @brief Check if a object has the `AML_OBJECT_EXCEPTION_ON_USE` flag set and raise an exception if it is.
 *
 * This will also clear the flag so the exception is only raised once.
 *
 * @param object Pointer to the object to check.
 * @param state The current AML state, used to raise the exception.
 */
void aml_object_exception_check(aml_object_t* object, aml_state_t* state);

/**
 * @brief Set a object as a buffer with the given content.
 *
 * @param object Pointer to the object to initialize.
 * @param buffer Pointer to the buffer.
 * @param bytesToCopy Number of bytes to copy from `buffer` to the object, the rest will be zeroed.
 * @param length The total length of the buffer.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_set(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length);

/**
 * @brief Set a object as an empty buffer with the given length.
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the buffer will also be the capacity.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_set_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Set a object as a buffer field with the given buffer, bit offset and bit size.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the object to create the buffer field from, must be `AML_BUFFER` or `AML_STRING`.
 * @param bitOffset Bit offset within the buffer.
 * @param bitSize Size of the field in bits.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_field_set(aml_object_t* object, aml_object_t* target, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Set a object as a debug object.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_debug_object_set(aml_object_t* object);

/**
 * @brief Set a object as a device or bus.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_device_set(aml_object_t* object);

/**
 * @brief Set a object as an event.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_event_set(aml_object_t* object);

/**
 * @brief Set a object as a field unit of type Field.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_field_set(aml_object_t* object, aml_opregion_obj_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Set a object as a field unit of type IndexField.
 *
 * @param object Pointer to the object to initialize.
 * @param index Pointer to the index field.
 * @param data Pointer to the data field.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_index_field_set(aml_object_t* object, aml_field_unit_obj_t* index, aml_field_unit_obj_t* data,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Set a object as a field unit of type BankField.
 *
 * @param object Pointer to the object to initialize.
 * @param opregion Pointer to the operation region.
 * @param bank Pointer to the bank field.
 * @param bankValue Value to write to the bank object to select the bank structure.
 * @param flags Flags for the field unit.
 * @param bitOffset Bit offset within the operation region.
 * @param bitSize Size of the field in bits.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_bank_field_set(aml_object_t* object, aml_opregion_obj_t* opregion, aml_field_unit_obj_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize);

/**
 * @brief Set a object as an integer with the given value and bit width.
 *
 * @param object Pointer to the object to initialize.
 * @param value The integer value to set.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_integer_set(aml_object_t* object, aml_integer_t value);

/**
 * @brief Set a object as a method with the given flags and address range.
 *
 * @param object Pointer to the object to initialize.
 * @param flags Flags for the method.
 * @param start Pointer to the start of the method's AML bytecode.
 * @param end Pointer to the end of the method's AML bytecode.
 * @param implementation Pointer to a C function that will execute the method, or `NULL` if the method is a normal
 * AML method.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_set(aml_object_t* object, aml_method_flags_t flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation);

/**
 * @brief Find the method which contains the provided address in its AML bytecode range.
 *
 * @param addr The address to search for.
 * @return On success, a reference to the found method object. On failure, `NULL` and `errno` is set.
 */
aml_method_obj_t* aml_method_find(const uint8_t* addr);

/**
 * @brief Set a object as a mutex with the given synchronization level.
 *
 * @param object Pointer to the object to initialize.
 * @param syncLevel The synchronization level of the mutex (0-15).
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_mutex_set(aml_object_t* object, aml_sync_level_t syncLevel);

/**
 * @brief Set a object as an ObjectReference to the given target object.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the ObjectReference will point to.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_reference_set(aml_object_t* object, aml_object_t* target);

/**
 * @brief Set a object as an operation region with the given space, offset, and length.
 *
 * @param object Pointer to the object to initialize.
 * @param space The address space of the operation region.
 * @param offset The offset within the address space.
 * @param length The length of the operation region.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_operation_region_set(aml_object_t* object, aml_region_space_t space, uintptr_t offset, uint32_t length);

/**
 * @brief Set a object as a package with the given number of elements.
 *
 * @param object Pointer to the object to initialize.
 * @param length Number of elements the package will be able to hold.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_set(aml_object_t* object, uint64_t length);

/**
 * @brief Set a object as a power resource with the given system level and resource order.
 *
 * @param object Pointer to the object to initialize.
 * @param systemLevel The system level of the power resource.
 * @param resourceOrder The resource order of the power resource.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_power_resource_set(aml_object_t* object, aml_system_level_t systemLevel,
    aml_resource_order_t resourceOrder);

/**
 * @brief Set a object as a processor with the given ProcID, PblkAddr, and PblkLen.
 *
 * @param object Pointer to the object to initialize.
 * @param procId The processor ID.
 * @param pblkAddr The pblk address.
 * @param pblkLen The length of the pblk.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_processor_set(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen);

/**
 * @brief Set a object as an empty string with the given length.
 *
 * The string will be initalized with zero chars and be null terminated.
 *
 * @param object Pointer to the object to initialize.
 * @param length Length of the string, not including the null terminator.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_set_empty(aml_object_t* object, uint64_t length);

/**
 * @brief Set a object as a string with the given value.
 *
 * @param object Pointer to the object to initialize.
 * @param str Pointer to the string.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_set(aml_object_t* object, const char* str);

/**
 * @brief Resize a string object to the new length.
 *
 * If the new length is greater than the current length, the new bytes will be initialized to zero.
 *
 * @param string Pointer to the string object to resize.
 * @param newLength The new length of the string, not including the null terminator.
 * @return On success, the new length of the string. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_resize(aml_string_obj_t* string, uint64_t newLength);

/**
 * @brief Set a object as a thermal zone.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_thermal_zone_set(aml_object_t* object);

/**
 * @brief Set a object as an alias to the given target object.
 *
 * This is used to implement the DefAlias structure.
 *
 * @param object Pointer to the object to initialize.
 * @param target Pointer to the target object the alias will point to.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_alias_set(aml_object_t* object, aml_object_t* target);

/**
 * @brief Traverse an alias object to get the target object.
 *
 * If the target is also an alias, it will be traversed recursively until a non-alias object is found.
 *
 * @param alias Pointer to the alias object to traverse.
 * @return On success, a reference to the target object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_alias_obj_traverse(aml_alias_obj_t* alias);

/**
 * @brief Set a object as an unresolved reference with the given namestring and starting point.
 *
 * The object will be resolved later by calling `aml_patch_up_resolve_all()`.
 *
 * @param object Pointer to the object to initialize.
 * @param nameString Pointer to the namestring representing the path to the target object.
 * @param from Pointer to the object to start the search from, can be `NULL` to start from the root.
 * @param callback Pointer to a callback function that will be called when a matching object is found
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_unresolved_set(aml_object_t* object, const aml_name_string_t* nameString, aml_object_t* from,
    aml_patch_up_resolve_callback_t callback);

/**
 * @brief Set a object as a predefined scope with the given name.
 *
 * This is used to implement predefined scopes like \_SB, \_GPE, etc.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_predefined_scope_set(aml_object_t* object);

/**
 * @brief Set a object as an argument with the given target object.
 *
 * @param object Pointer to the object to initialize.
 * @param value Pointer to the object the argument will point to, can be `NULL`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_arg_set(aml_object_t* object, aml_object_t* value);

/**
 * @brief Set a object as a empty local variable.
 *
 * @param object Pointer to the object to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_local_set(aml_object_t* object);

/** @} */
