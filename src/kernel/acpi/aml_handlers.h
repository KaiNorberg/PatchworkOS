#pragma once

#include <errno.h>
#include <stdint.h>

#include "log/log.h"
#include "aml_state.h"

/**
 * @brief Handles the ScopeOp AML opcode.
 *
 * See section 20.2.5.1 of the ACPI specification.
 *
 * @param state The AML state.
 * @return uint64_t On success, 0. On error, `ERR` and `errno` is set.
 */
static uint64_t aml_handler_scope_op(aml_state_t* state)
{
    aml_pkg_length_t pkgLength;
    if (aml_state_read_pkg_length(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_state_read_name_string(state, &nameString) == ERR)
    {
        return ERR;
    }

    LOG_INFO("ScopeOp pkgLength: %zu, segments: %zu\n", pkgLength, nameString.segmentCount);
    for (size_t i = 0; i < nameString.segmentCount; i++)
    {
        LOG_INFO("Segment %zu: %.4s\n", i, nameString.segments[i]);
    }

    return 0;
}
