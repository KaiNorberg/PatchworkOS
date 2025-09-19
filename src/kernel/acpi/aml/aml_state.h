#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "encoding/data_integers.h"
#include "encoding/data_object.h"
#include "encoding/term.h"

/**
 * @brief ACPI AML State
 * @defgroup kernel_acpi_aml_parse State
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
 * Used in the `aml_parse()` function to keep track of the virtual machine's state.
 *
 * Note that when a Method is evaluated a new `aml_state_t` is created for the Method's AML bytecode stream.
 */
typedef struct aml_state
{
    const void* data;          //!< Pointer to the AML bytecode stream.
    uint64_t dataSize;         //!< Size of the AML bytecode stream.
    aml_address_t pos;         //!< Index of the current instruction in `data`.
    aml_term_arg_list_t* args; //!< Pointer to the current local arguments, can be NULL.
} aml_state_t;

static inline void aml_state_init(aml_state_t* state, const void* data, uint64_t dataSize, aml_term_arg_list_t* args)
{
    state->data = data;
    state->dataSize = dataSize;
    state->pos = 0;
    state->args = args;
}

static inline void aml_state_deinit(aml_state_t* state)
{
    state->data = NULL;
    state->dataSize = 0;
    state->pos = 0;
}

static inline uint64_t aml_state_read(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->pos;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->pos, count);
    state->pos += count;
    return count;
}

static inline uint64_t aml_state_peek(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->pos;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->pos, count);
    return count;
}

static inline uint64_t aml_state_advance(aml_state_t* state, uint64_t offset)
{
    uint64_t bytesAvailable = state->dataSize - state->pos;
    if (offset > bytesAvailable)
    {
        offset = bytesAvailable;
    }

    state->pos += offset;
    return state->pos;
}

/** @} */
