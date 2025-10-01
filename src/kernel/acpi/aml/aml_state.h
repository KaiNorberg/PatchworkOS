#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "acpi/aml/aml_object.h"
#include "encoding/local.h"
#include "encoding/term.h"
#include "runtime/mutex.h"

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
 * @brief Flow control types.
 * @enum aml_flow_control_t
 */
typedef enum
{
    AML_FLOW_CONTROL_EXECUTE, //!< Normal execution
    AML_FLOW_CONTROL_RETURN,  //!< A Return statement was hit
    AML_FLOW_CONTROL_BREAK,   //!< A Break statement was hit
    AML_FLOW_CONTROL_CONTINUE //!< A Continue statement was hit
} aml_flow_control_t;

/**
 * @brief AML State
 * @struct aml_state_t
 *
 * Used in the `aml_parse()` function to keep track of the virtual machine's state and while invoking methods.
 *
 * Note that when a Method is evaluated a new `aml_state_t` is created for the Method's AML bytecode stream.
 */
typedef struct aml_state
{
    const uint8_t* start;                //!< Pointer to the start of the AML bytecode.
    const uint8_t* end;                  //!< Pointer to the end of the AML bytecode.
    const uint8_t* current;              //!< Pointer to the current position in the AML bytecode.
    aml_object_t locals[AML_MAX_LOCALS]; //!< Local variables for the method, if any.
    aml_term_arg_list_t* args; //!< Arguments passed to the method, if the state is used for method evaluation.
    aml_object_t* returnValue; //!< Pointer to where the return value should be stored, if the state is for a method.
    const uint8_t* lastErrPos; //!<  The position when the last error occurred.
    aml_flow_control_t flowControl; //!< Used by `aml_term_list_read` to handle flow control statements
    aml_mutex_stack_t mutexStack;   //!< Handles acquiring and releasing mutexes.
} aml_state_t;

/**
 * @brief Initialize an AML state.
 *
 * @param state Pointer to the state to initialize.
 * @param start Pointer to the start of the AML bytecode.
 * @param end Pointer to the end of the AML bytecode.
 * @param args Pointer to the arguments passed to the method, or `NULL` if not a method or no arguments.
 * @param returnValue Pointer to where the return value should be stored, or `NULL` if not a method or no return value.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_state_init(aml_state_t* state, const uint8_t* start, const uint8_t* end, aml_term_arg_list_t* args,
    aml_object_t* returnValue);

/**
 * @brief Deinitialize an AML state.
 *
 * Will error if the state is holding any mutexes or if `flowControl` is not `AML_FLOW_CONTROL_EXECUTE` or
 * `AML_FLOW_CONTROL_RETURN`.
 *
 * Even if an error occurs all resources will still be freed.
 *
 * @param state Pointer to the state to deinitialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_state_deinit(aml_state_t* state);

static inline uint64_t aml_state_read(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->end - state->current;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, state->current, count);
    state->current += count;
    return count;
}

static inline uint64_t aml_state_peek(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->end - state->current;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, state->current, count);
    return count;
}

static inline uint64_t aml_state_advance(aml_state_t* state, uint64_t offset)
{
    uint64_t bytesAvailable = state->end - state->current;
    if (offset > bytesAvailable)
    {
        offset = bytesAvailable;
    }

    state->current += offset;
    return offset;
}

/** @} */
