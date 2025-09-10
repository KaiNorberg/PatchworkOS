#pragma once

#include "aml_node.h"
#include "aml_state.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
 */
typedef struct aml_state
{
    const void* data;            //!< Pointer to the AML bytecode stream.
    uint64_t dataSize;           //!< Size of the AML bytecode stream.
    uint64_t instructionPointer; //!< Index of the current instruction in `data`.
} aml_state_t;

static inline void aml_state_init(aml_state_t* state, const void* data, uint64_t dataSize)
{
    state->data = data;
    state->dataSize = dataSize;
    state->instructionPointer = 0;
}

static inline void aml_state_deinit(aml_state_t* state)
{
    state->data = NULL;
    state->dataSize = 0;
    state->instructionPointer = 0;
}

static inline uint64_t aml_state_byte_read(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer++];
}

static inline uint64_t aml_state_byte_peek(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer];
}

static inline uint64_t aml_state_bytes_read(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->instructionPointer, count);
    state->instructionPointer += count;
    return count;
}

static inline uint64_t aml_state_bytes_peek(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->instructionPointer, count);
    return count;
}

static inline uint64_t aml_state_advance(aml_state_t* state, uint64_t offset)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (offset > bytesAvailable)
    {
        offset = bytesAvailable;
    }

    state->instructionPointer += offset;
    return state->instructionPointer;
}

/** @} */
