#include <kernel/acpi/aml/encoding/namespace_modifier.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/name.h>
#include <kernel/acpi/aml/encoding/package_length.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

status_t aml_def_alias_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_ALIAS_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read AliasOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* source;
    status_t status = aml_name_string_read_and_resolve(ctx, &source);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve source NameString");
        return status;
    }
    UNREF_DEFER(source);

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve target NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_alias_set(newObject, source);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set alias object");
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add alias object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_def_name_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_NAME_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_name_string_t nameString;
    status_t status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_data_ref_object_read(ctx, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DataRefObject");
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_def_scope_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_SCOPE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ScopeOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_object_t* scope;
    status = aml_name_string_read_and_resolve(ctx, &scope);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return status;
    }
    UNREF_DEFER(scope);

    const uint8_t* end = start + pkgLength;

    aml_type_t type = scope->type;
    if (type != AML_PREDEFINED_SCOPE && type != AML_DEVICE && type != AML_PROCESSOR && type != AML_THERMAL_ZONE &&
        type != AML_POWER_RESOURCE)
    {
        AML_DEBUG_ERROR(ctx, "Invalid object type '%s'", aml_type_to_string(type));
        return ERR(ACPI, ILSEQ);
    }

    status = aml_term_list_read(ctx->state, scope, ctx->current, end, ctx);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermList");
        return status;
    }
    ctx->current = end;

    return 0;
}

status_t aml_namespace_modifier_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t status = OK;
    switch (token.num)
    {
    case AML_ALIAS_OP:
        status = aml_def_alias_read(ctx);
        break;
    case AML_NAME_OP:
        status = aml_def_name_read(ctx);
        break;
    case AML_SCOPE_OP:
        status = aml_def_scope_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid NamespaceModifierObj '0x%x'", token.num);
        status = ERR(ACPI, ILSEQ);
        break;
    }

    return status;
}
