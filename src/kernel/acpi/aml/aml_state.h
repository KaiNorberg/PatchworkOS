#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "acpi/aml/aml_node.h"
#include "encoding/arg.h"
#include "encoding/data_integers.h"
#include "encoding/local.h"
#include "encoding/term.h"
#include "log/log.h"

/**
 * @brief State
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
    const uint8_t* start;           //!< Pointer to the start of the AML bytecode.
    const uint8_t* end;             //!< Pointer to the end of the AML bytecode.
    const uint8_t* current;         //!< Pointer to the current position in the AML bytecode.
    aml_node_t* args[AML_MAX_ARGS]; //!< Argument variables, only used if the state is being used to evaluate a method.
    aml_node_t* locals[AML_MAX_LOCALS]; //!< Local variables for the method, if any.
    struct
    {
        const uint8_t* lastErrPos; //!<  The position when the last error occurred.
    } debug;
} aml_state_t;

static inline uint64_t aml_state_init(aml_state_t* state, const uint8_t* start, const uint8_t* end)
{
    if (start >= end)
    {
        LOG_ERR("Invalid AML data start >= end");
        return ERR;
    }

    state->start = start;
    state->end = end;
    state->current = start;

    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        state->args[i] = NULL;
    }
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        state->locals[i] = NULL;
    }
    state->debug.lastErrPos = NULL;
    return 0;
}

static inline void aml_state_deinit(aml_state_t* state)
{
    state->start = NULL;
    state->end = NULL;
    state->current = NULL;

    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        aml_node_free(state->args[i]);
        state->args[i] = NULL;
    }
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        aml_node_free(state->locals[i]);
        state->locals[i] = NULL;
    }
    state->debug.lastErrPos = NULL;
}

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
