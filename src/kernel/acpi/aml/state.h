#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "acpi/aml/object.h"
#include "encoding/arg.h"
#include "encoding/local.h"

/**
 * @brief State
 * @defgroup kernel_acpi_aml_state State
 * @ingroup kernel_acpi_aml
 *
 * The ACPI AML State is used to keep track of the virtual machine's state during the parsing of AML bytecode and
 * provides wrappers to read data from the ACPI AML stream.
 *
 * @{
 */

/**
 * @brief AML State
 * @struct aml_state_t
 *
 * Used to keep track of the virtual machine's state and while invoking methods or parsing DSDT/SSDT tables.
 *
 * Note that when a Method is evaluated a new `aml_state_t` is created for the Method's AML bytecode stream.
 */
typedef struct aml_state
{
    aml_local_obj_t* locals[AML_MAX_LOCALS]; ///< Local variables for the method, if any.
    aml_arg_obj_t* args[AML_MAX_ARGS];       ///< Argument variables for the method, if any.
    aml_object_t* result;                    ///< The return value, see `aml_method_evaluate()` for details.
    uint64_t errorDepth;                     ///< The length of the error traceback, if 0 then no error has occurred.
    /**
     * List of named objects added to the namespace as the state was executing. These objects should be removed from the
     * namespace if the state was used to execute a method, via the `aml_state_garbage_collect()` function.
     *
     * If the state was not used to execute a method, instead it was used to parse a DSDT or SSDT table,
     * then the states created objects should not be removed, as they are now part a permanent part of the ACPI
     * namespace.
     *
     * The state does not take a reference to the objects, it just keeps track of them for garbage collection.
     */
    list_t namedObjects;
} aml_state_t;

/**
 * @brief Initialize an AML state.
 *
 * @param state Pointer to the state to initialize.
 * @param args Array of pointers to the objects to pass as arguments, or `NULL` if not a method or no arguments.
 * @param argCount Number of arguments, or 0 if not a method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_state_init(aml_state_t* state, aml_object_t** args, uint64_t argCount);

/**
 * @brief Deinitialize an AML state.
 *
 * Will not free any objects created by the state as that is not always wanted, for example when the state was used to
 * parse a DSDT or SSDT table. Use `aml_state_garbage_collect()` to free all objects created by the state.
 *
 * @param state Pointer to the state to deinitialize.
 */
void aml_state_deinit(aml_state_t* state);

/**
 * @brief Free all objects created by the state.
 *
 * @param state Pointer to the state to garbage collect.
 */
void aml_state_garbage_collect(aml_state_t* state);

/**
 * @brief Get the result object of the state.
 *
 * If no result is available, a Integer object with value 0 is returned.
 *
 * @see aml_method_evaluate() for more details.
 *
 * @param state Pointer to the state.
 * @return On success, a copy of the result object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_state_result_get(aml_state_t* state);

/**
 * @brief Set the result object of the state.
 *
 * Note that methods are supposed to return copies of objects, not references to existing objects. However, due to the
 * behaviour of implicit returns, always returning the last evaluated object, means there is no way for the object to be
 * modified between this function being called and the method returning. So in this case it is acceptable to store a
 * reference to the object and only make a copy when the method actually returns.
 *
 * @see aml_method_evaluate() for more details.
 *
 * @param state Pointer to the state.
 * @param result Pointer to the result object, or `NULL` to clear the result.
 */
void aml_state_result_set(aml_state_t* state, aml_object_t* result);

/** @} */
