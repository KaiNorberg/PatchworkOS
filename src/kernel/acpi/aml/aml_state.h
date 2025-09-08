#pragma once

#include "aml_state.h"
#include "fs/path.h"

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

#define AML_MAX_CONTEXT_DEPTH 32 //!< Maximum depth of the AML context stack.

/**
 * @brief AML Context
 * @struct aml_context_t
 *
 * AML is formatted hierarchically, where each context represents a scope, namespace, etc.
 */
typedef struct
{
    path_t path; //!< The current location within the `/acpi/` SysFS group, for example `/acpi/_SB/PCI0`.
} aml_context_t;

static inline void aml_context_init(aml_context_t* context, const path_t* path)
{
    if (path == NULL)
    {
        context->path = PATH_EMPTY;
    }
    else
    {
        path_copy(&context->path, path);
    }
}

static inline void aml_context_deinit(aml_context_t* context)
{
    path_put(&context->path);
}

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
    uint64_t instructionPointer; //!< Index of the current instruction in the AML bytecode stream.
    aml_context_t contextStack[AML_MAX_CONTEXT_DEPTH]; //!< The stack of AML contexts.
    uint64_t contextDepth; //!< How many contexts are currently on the stack.
} aml_state_t;

static inline void aml_state_init(aml_state_t* state, const void* data, uint64_t dataSize)
{
    state->data = data;
    state->dataSize = dataSize;
    state->instructionPointer = 0;
    state->contextDepth = 0;
    for (uint64_t i = 0; i < AML_MAX_CONTEXT_DEPTH; i++)
    {
        aml_context_init(&state->contextStack[i], NULL);
    }
}

static inline void aml_state_deinit(aml_state_t* state)
{
    for (uint64_t i = 0; i < state->contextDepth; i++)
    {
        aml_context_deinit(&state->contextStack[i]);
    }

    state->data = NULL;
    state->dataSize = 0;
    state->instructionPointer = 0;
    state->contextDepth = 0;
}

static inline aml_context_t* aml_state_context_get(aml_state_t* state)
{
    if (state->contextDepth == 0)
    {
        return NULL;
    }

    return &state->contextStack[state->contextDepth - 1];
}

static inline uint64_t aml_byte_read(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer++];
}

static inline uint64_t aml_byte_peek(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer];
}

static inline uint64_t aml_bytes_read(aml_state_t* state, uint8_t* buffer, uint64_t count)
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

static inline uint64_t aml_bytes_peek(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->instructionPointer, count);
    return count;
}

static inline uint64_t aml_advance(aml_state_t* state, uint64_t offset)
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
