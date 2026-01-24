#include <kernel/acpi/aml/predefined.h>

#include <kernel/log/log.h>
#include <kernel/acpi/acpi.h>
#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/object.h>

#include <kernel/version.h>

#include <errno.h>

static aml_mutex_t* globalMutex = NULL;

aml_object_t* aml_osi_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount)
{
    UNUSED(method);

    if (argCount != 1 || args[0]->type != AML_STRING)
    {
        errno = EINVAL;
        return NULL;
    }

    LOG_DEBUG("_OSI called with argument: '%.*s'\n", (int)args[0]->string.length, args[0]->string.content);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }

    /// @todo Implement _OSI strings properly. For now, we just return true for everything.

    if (aml_integer_set(result, UINT64_MAX) == ERR)
    {
        UNREF(result);
        return NULL;
    }

    return result;
}

aml_object_t* aml_rev_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount)
{
    UNUSED(method);
    UNUSED(args); // Unused

    if (argCount != 0)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }

    if (aml_integer_set(result, RSDP_CURRENT_REVISION) == ERR)
    {
        UNREF(result);
        return NULL;
    }

    return result;
}

aml_object_t* aml_os_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount)
{
    UNUSED(method);
    UNUSED(args); // Unused

    if (argCount != 0)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }

    if (aml_string_set(result, OS_NAME) == ERR)
    {
        UNREF(result);
        return NULL;
    }

    return result;
}

static inline uint64_t aml_create_predefined_scope(aml_name_t name)
{
    aml_object_t* object = aml_object_new();
    if (object == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(object);

    if (aml_predefined_scope_set(object) == ERR || aml_namespace_add_child(NULL, NULL, name, object) == ERR)
    {
        return ERR;
    }

    return 0;
}

aml_mutex_t* aml_gl_get(void)
{
    return globalMutex;
}

uint64_t aml_predefined_init(void)
{
    // Normal predefined root objects, see section 5.3.1 of the ACPI specification.
    if (aml_create_predefined_scope(AML_NAME('_', 'G', 'P', 'E')) == ERR ||
        aml_create_predefined_scope(AML_NAME('_', 'P', 'R', '_')) == ERR ||
        aml_create_predefined_scope(AML_NAME('_', 'S', 'B', '_')) == ERR ||
        aml_create_predefined_scope(AML_NAME('_', 'S', 'I', '_')) == ERR ||
        aml_create_predefined_scope(AML_NAME('_', 'T', 'Z', '_')) == ERR)
    {
        return ERR;
    }

    // OS specific predefined objects, see section 5.7 of the ACPI specification.
    aml_object_t* osi = aml_object_new();
    if (osi == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(osi);
    aml_method_flags_t osiFlags = {
        .argCount = 1,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(osi, osiFlags, NULL, NULL, aml_osi_implementation) == ERR ||
        aml_namespace_add_child(NULL, NULL, AML_NAME('_', 'O', 'S', 'I'), osi) == ERR)
    {
        return ERR;
    }

    aml_object_t* rev = aml_object_new();
    if (rev == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(rev);
    aml_method_flags_t revFlags = {
        .argCount = 0,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(rev, revFlags, NULL, NULL, aml_rev_implementation) == ERR ||
        aml_namespace_add_child(NULL, NULL, AML_NAME('_', 'R', 'E', 'V'), rev) == ERR)
    {
        return ERR;
    }

    aml_object_t* os = aml_object_new();
    if (os == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(os);
    aml_method_flags_t osFlags = {
        .argCount = 0,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(os, osFlags, NULL, NULL, aml_os_implementation) == ERR ||
        aml_namespace_add_child(NULL, NULL, AML_NAME('_', 'O', 'S', '_'), os) == ERR)
    {
        return ERR;
    }

    /// @todo Implement _GL properly.
    aml_object_t* gl = aml_object_new();
    if (gl == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(gl);
    if (aml_mutex_set(gl, 0) == ERR || aml_namespace_add_child(NULL, NULL, AML_NAME('_', 'G', 'L', '_'), gl) == ERR)
    {
        return ERR;
    }

    globalMutex = REF(&gl->mutex);
    return 0;
}
