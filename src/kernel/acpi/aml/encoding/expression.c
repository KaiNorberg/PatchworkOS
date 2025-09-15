#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out)
{
    aml_term_arg_t termArg;
    if (aml_term_arg_read(state, NULL, &termArg, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }
    *out = termArg.integer;
    return 0;
}

uint64_t aml_def_buffer_read(aml_state_t* state, uint8_t** out, uint64_t* outLength)
{
    aml_address_t start = state->pos;

    aml_value_t bufferOp;
    if (aml_value_read(state, &bufferOp) == ERR)
    {
        return ERR;
    }

    if (bufferOp.num != AML_BUFFER_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&bufferOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    if (pkgLength < 1)
    {
        AML_DEBUG_INVALID_STRUCTURE("DefBuffer: Buffer length must be at least 1");
        errno = EILSEQ;
        return ERR;
    }

    aml_buffer_size_t bufferSize;
    if (aml_buffer_size_read(state, &bufferSize) == ERR)
    {
        return ERR;
    }

    // TODO: Im not sure why we have both pkgLength and bufferSize, but for now we just check they match. In the future if this causes an error we can figure it out from there.

    aml_address_t end = start + pkgLength;
    if (end + 1 != state->pos + bufferSize)
    {
        LOG_ERR("pkgLength: %llu, bufferSize: %llu, calculated end: 0x%llx, actual end: 0x%llx\n", pkgLength, bufferSize, state->pos + bufferSize, end);
        AML_DEBUG_INVALID_STRUCTURE("DefBuffer: Mismatch between PkgLength and BufferSize, unsure if this is valid");
        errno = ENOSYS;
        return ERR;
    }

    *out = (uint8_t*)(state->data + state->pos);
    *outLength = bufferSize;
    aml_state_advance(state, bufferSize);
    return 0;
}
