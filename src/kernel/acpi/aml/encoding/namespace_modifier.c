#include "namespace_modifier.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_token.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

uint64_t aml_def_alias_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_ALIAS_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read AliasOp");
        return ERR;
    }

    aml_object_t* source = aml_name_string_read_and_resolve(ctx);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve source NameString");
        return ERR;
    }
    DEREF_DEFER(source);

    aml_name_string_t targetNameString;
    if (aml_name_string_read(ctx, &targetNameString) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve target NameString");
        return ERR;
    }

    aml_object_t* target = aml_object_new(ctx);
    if (target == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }
    DEREF_DEFER(target);

    if (aml_alias_set(target, source) == ERR || aml_object_add(target, ctx->scope, &targetNameString) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add alias object '%s'", aml_name_string_to_string(&targetNameString));
        return ERR;
    }

    return 0;
}

uint64_t aml_def_name_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_NAME_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameOp");
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(ctx, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return ERR;
    }

    aml_object_t* newObject = aml_object_new(ctx);
    if (newObject == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }
    DEREF_DEFER(newObject);

    if (aml_data_ref_object_read(ctx, newObject) == ERR ||
        aml_object_add(newObject, ctx->scope, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return ERR;
    }

    return 0;
}

uint64_t aml_def_scope_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_SCOPE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ScopeOp");
        return ERR;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return ERR;
    }

    aml_object_t* scope = aml_name_string_read_and_resolve(ctx);
    if (scope == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return ERR;
    }
    DEREF_DEFER(scope);

    const uint8_t* end = start + pkgLength;

    aml_type_t type = scope->type;
    if (type != AML_PREDEFINED_SCOPE && type != AML_DEVICE && type != AML_PROCESSOR && type != AML_THERMAL_ZONE &&
        type != AML_POWER_RESOURCE)
    {
        AML_DEBUG_ERROR(ctx, "Invalid object type '%s'", aml_type_to_string(type));
        errno = EILSEQ;
        return ERR;
    }

    if (aml_term_list_read(ctx->state, scope, ctx->current, end, ctx) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermList");
        return ERR;
    }
    ctx->current = end;

    return 0;
}

uint64_t aml_namespace_modifier_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    switch (token.num)
    {
    case AML_ALIAS_OP:
        if (aml_def_alias_read(ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read DefAlias");
            return ERR;
        }
        return 0;
    case AML_NAME_OP:
        if (aml_def_name_read(ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read DefName");
            return ERR;
        }
        return 0;
    case AML_SCOPE_OP:
        if (aml_def_scope_read(ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read DefScope");
            return ERR;
        }
        return 0;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid NamespaceModifierObj '0x%x'", token.num);
        errno = EILSEQ;
        return ERR;
    }
}
