#include <kernel/acpi/aml/encoding/data.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/encoding/name.h>
#include <kernel/acpi/aml/encoding/package_length.h>
#include <kernel/acpi/aml/patch_up.h>
#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>

#include <sys/status.h>

#include <errno.h>
#include <stdint.h>

status_t aml_byte_data_read(aml_term_list_ctx_t* ctx, uint8_t* out)
{
    if (ctx->end - ctx->current < 1)
    {
        AML_DEBUG_ERROR(ctx, "Not enough data to read ByteData");
        return ERR(ACPI, ILSEQ);
    }

    *out = *ctx->current;
    ctx->current += 1;
    return OK;
}

status_t aml_word_data_read(aml_term_list_ctx_t* ctx, uint16_t* out)
{
    uint8_t byte1, byte2;
    status_t status = aml_byte_data_read(ctx, &byte1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read word data");
        return status;
    }
    status = aml_byte_data_read(ctx, &byte2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read word data");
        return status;
    }
    *out = ((uint16_t)byte1) | (((uint16_t)byte2) << 8);
    return OK;
}

status_t aml_dword_data_read(aml_term_list_ctx_t* ctx, uint32_t* out)
{
    uint16_t word1, word2;
    status_t status = aml_word_data_read(ctx, &word1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read dword data");
        return status;
    }
    status = aml_word_data_read(ctx, &word2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read dword data");
        return status;
    }
    *out = ((uint32_t)word1) | (((uint32_t)word2) << 16);
    return OK;
}

status_t aml_qword_data_read(aml_term_list_ctx_t* ctx, uint64_t* out)
{
    uint32_t dword1, dword2;
    status_t status = aml_dword_data_read(ctx, &dword1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read qword data");
        return status;
    }
    status = aml_dword_data_read(ctx, &dword2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read qword data");
        return status;
    }
    *out = ((uint64_t)dword1) | (((uint64_t)dword2) << 32);
    return OK;
}

status_t aml_byte_const_read(aml_term_list_ctx_t* ctx, uint8_t* out)
{
    status_t status = aml_token_expect(ctx, AML_BYTE_PREFIX);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read byte prefix");
        return status;
    }

    return aml_byte_data_read(ctx, out);
}

status_t aml_word_const_read(aml_term_list_ctx_t* ctx, uint16_t* out)
{
    status_t status = aml_token_expect(ctx, AML_WORD_PREFIX);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read word prefix");
        return status;
    }

    return aml_word_data_read(ctx, out);
}

status_t aml_dword_const_read(aml_term_list_ctx_t* ctx, uint32_t* out)
{
    status_t status = aml_token_expect(ctx, AML_DWORD_PREFIX);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read dword prefix");
        return status;
    }

    return aml_dword_data_read(ctx, out);
}

status_t aml_qword_const_read(aml_term_list_ctx_t* ctx, uint64_t* out)
{
    status_t status = aml_token_expect(ctx, AML_QWORD_PREFIX);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read qword prefix");
        return status;
    }

    return aml_qword_data_read(ctx, out);
}

status_t aml_const_obj_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    aml_token_t token;
    aml_token_read(ctx, &token);

    status_t status = OK;
    switch (token.num)
    {
    case AML_ZERO_OP:
        status = aml_integer_set(out, 0);
        break;
    case AML_ONE_OP:
        status = aml_integer_set(out, 1);
        break;
    case AML_ONES_OP:
        status = aml_integer_set(out, aml_integer_ones());
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid ConstObj token '0x%x'", token.num);
        return ERR(ACPI, ILSEQ);
    }

    return status;
}

status_t aml_string_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status = aml_token_expect(ctx, AML_STRING_PREFIX);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read StringPrefix");
        return status;
    }

    const char* start = (const char*)ctx->current;
    while (1)
    {
        uint8_t c;
        status = aml_byte_data_read(ctx, &c);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read byte in string");
            return status;
        }

        if (c == 0x00)
        {
            break;
        }

        if (c < 0x01 || c > 0x7F)
        {
            AML_DEBUG_ERROR(ctx, "Invalid ASCII character '0x%x' in string", c);
            return ERR(ACPI, ILSEQ);
        }
    }

    status = aml_string_set(out, start);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

status_t aml_revision_op_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status = aml_token_expect(ctx, AML_REVISION_OP);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RevisionOp");
        return status;
    }

    status = aml_integer_set(out, AML_CURRENT_REVISION);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

status_t aml_computational_data_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    switch (token.num)
    {
    case AML_BYTE_PREFIX:
    {
        uint8_t byte;
        status_t status = aml_byte_const_read(ctx, &byte);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ByteConst");
            return status;
        }

        status = aml_integer_set(out, byte);
        if (IS_ERR(status))
        {
            return status;
        }
        return OK;
    }
    case AML_WORD_PREFIX:
    {
        uint16_t word;
        status_t status = aml_word_const_read(ctx, &word);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read WordConst");
            return status;
        }

        return aml_integer_set(out, word);
        if (IS_ERR(status))
        {
            return status;
        }
        return OK;
    }
    case AML_DWORD_PREFIX:
    {
        uint32_t dword;
        status_t status = aml_dword_const_read(ctx, &dword);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read DWordConst");
            return status;
        }

        status = aml_integer_set(out, dword);
        if (IS_ERR(status))
        {
            return status;
        }
        return OK;
    }
    case AML_QWORD_PREFIX:
    {
        uint64_t qword;
        status_t status = aml_qword_const_read(ctx, &qword);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read QWordConst");
            return status;
        }

        status = aml_integer_set(out, qword);
        if (IS_ERR(status))
        {
            return status;
        }
        return OK;
    }
    case AML_STRING_PREFIX:
    {
        status_t status = aml_string_read(ctx, out);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read String");
            return status;
        }
        return OK;
    }
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
    {
        status_t status = aml_const_obj_read(ctx, out);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ConstObj");
            return status;
        }
        return OK;
    }
    case AML_BUFFER_OP:
    {
        status_t status = aml_def_buffer_read(ctx, out);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read Buffer");
            return status;
        }
        return OK;
    }
    case AML_REVISION_OP:
    {
        status_t status = aml_revision_op_read(ctx, out);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read RevisionOp");
            return status;
        }
        return OK;
    }
    default:
        AML_DEBUG_ERROR(ctx, "Invalid ComputationalData '%s' (0x%x)", token.props->name, token.num);
        return ERR(ACPI, ILSEQ);
    }
}

status_t aml_num_elements_read(aml_term_list_ctx_t* ctx, uint8_t* out)
{
    status_t status = aml_byte_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NumElements");
        return status;
    }

    return OK;
}

/**
 * Used to handle package elements that are names but mainly its used as a callback for
 * the @ref modules_acpi_aml_patch_up system.
 */
static inline status_t aml_package_element_handle_name(aml_state_t* state, aml_object_t* in, aml_object_t* out)
{
    if (in->type &
        (AML_INTEGER | AML_STRING | AML_BUFFER | AML_BUFFER_FIELD | AML_FIELD_UNIT |
            AML_PACKAGE)) // "... resolved to actual data by the AML interpreter"
    {
        // Unsure what the spec means by "actual data" but converting to DataObject seems to be the most sensible
        // interpretation.
        status_t status = aml_convert_source(state, in, &out, AML_DATA_OBJECTS);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to convert to data object in aml_package_element_handle_name()\n");
            return status;
        }
        return OK;
    }

    // "... returned in the package as references"

    status_t status = aml_object_reference_set(out, in);
    if (IS_ERR(status))
    {
        LOG_ERR("failed to init ObjectReference in aml_package_element_handle_name()\n");
        return status;
    }
    return OK;
}

status_t aml_package_element_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    if (token.props->type == AML_TOKEN_TYPE_NAME)
    {
        aml_name_string_t nameString;
        status_t status = aml_name_string_read(ctx, &nameString);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read NameString");
            return ERR(ACPI, ILSEQ);
        }

        aml_object_t* object = aml_namespace_find_by_name_string(&ctx->state->overlay, ctx->scope, &nameString);
        if (object == NULL)
        {
            status = aml_unresolved_set(out, &nameString, ctx->scope, aml_package_element_handle_name);
            if (IS_ERR(status))
            {
                return status;
            }
            return OK;
        }

        status = aml_package_element_handle_name(ctx->state, object, out);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to handle name in PackageElement");
            return ERR(ACPI, ILSEQ);
        }

        return OK;
    }

    status_t status = aml_data_ref_object_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DataRefObject");
        return status;
    }

    return OK;
}

status_t aml_package_element_list_read(aml_term_list_ctx_t* ctx, aml_package_t* package, const uint8_t* end)
{
    uint64_t i = 0;
    while (ctx->current < end && i < package->length)
    {
        status_t status = aml_package_element_read(ctx, package->elements[i]);
        if (IS_ERR(status))
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_clear(package->elements[j]);
            }
            AML_DEBUG_ERROR(ctx, "Failed to read PackageElement %llu", i);
            return status;
        }
        i++;
    }

    return OK;
}

status_t aml_def_package_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status = aml_token_expect(ctx, AML_PACKAGE_OP);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PackageOp");
        return status;
    }

    const uint8_t* start = ctx->current;

    // PkgLength specifies how many elements in the package are defined, others are left uninitialized.
    aml_pkg_length_t pkgLength;
    status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* end = start + pkgLength;

    // NumElements specifies the capacity of the package.
    uint8_t numElements;
    status = aml_num_elements_read(ctx, &numElements);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_package_set(out, numElements);
    if (IS_ERR(status))
    {
        return ERR(ACPI, NOMEM);
    }

    status = aml_package_element_list_read(ctx, &out->package, end);
    if (IS_ERR(status))
    {
        aml_object_clear(out);
        AML_DEBUG_ERROR(ctx, "Failed to read PackageElementList");
        return status;
    }

    return OK;
}

status_t aml_def_var_num_elements_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg for VarNumElements");
        return status;
    }

    return OK;
}

status_t aml_def_var_package_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status = aml_token_expect(ctx, AML_VAR_PACKAGE_OP);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read VarPackageOp");
        return status;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    uint64_t numElements;
    status = aml_def_var_num_elements_read(ctx, &numElements);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read VarNumElements");
        return status;
    }

    status = aml_package_set(out, numElements);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_package_element_list_read(ctx, &out->package, end);
    if (IS_ERR(status))
    {
        aml_object_clear(out);
        AML_DEBUG_ERROR(ctx, "Failed to read PackageElementList");
        return status;
    }

    return OK;
}

status_t aml_data_object_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t result = OK;
    switch (token.num)
    {
    case AML_PACKAGE_OP:
        result = aml_def_package_read(ctx, out);
        break;
    case AML_VAR_PACKAGE_OP:
        result = aml_def_var_package_read(ctx, out);
        break;
    default:
        result = aml_computational_data_read(ctx, out);
        break;
    }

    if (IS_ERR(result))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", token.props->name);
        return result;
    }

    return OK;
}

status_t aml_data_ref_object_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status = aml_data_object_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DataObject");
        return status;
    }

    return OK;
}
