#include "aml_predefined.h"

#include "acpi/acpi.h"
#include "aml.h"
#include "aml_object.h"
#include "log/log.h"

#include <common/version.h>

#include <errno.h>

static aml_mutex_obj_t* globalMutex = NULL;

uint64_t aml_osi_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* returnValue)
{
    (void)method; // Unused

    if (argCount != 1 || args[0]->type != AML_STRING)
    {
        errno = EINVAL;
        return ERR;
    }

    LOG_DEBUG("_OSI called with argument: '%.*s'\n", (int)args[0]->string.length, args[0]->string.content);

    // TODO: Implement this properly.
    if (aml_integer_set(returnValue, UINT64_MAX) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_rev_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* returnValue)
{
    (void)method; // Unused
    (void)args;   // Unused

    if (argCount != 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_integer_set(returnValue, ACPI_REVISION) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_os_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* returnValue)
{
    (void)method; // Unused
    (void)args;   // Unused

    if (argCount != 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_string_set(returnValue, OS_NAME) == ERR)
    {
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_create_predefined_scope(const char* name)
{
    aml_object_t* root = aml_root_get();
    assert(root != NULL);

    aml_object_t* object = aml_object_new(NULL, AML_OBJECT_PREDEFINED);
    if (object == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(object);

    if (aml_predefined_scope_set(object) == ERR || aml_object_add_child(root, object, name) == ERR)
    {
        return ERR;
    }

    return 0;
}

aml_mutex_obj_t* aml_gl_get(void)
{
    return globalMutex;
}

uint64_t aml_predefined_init(void)
{
    // Normal predefined root objects, see section 5.3.1 of the ACPI specification.
    if (aml_create_predefined_scope("_GPE") == ERR || aml_create_predefined_scope("_PR_") == ERR ||
        aml_create_predefined_scope("_SB_") == ERR || aml_create_predefined_scope("_SI_") == ERR ||
        aml_create_predefined_scope("_TZ_") == ERR)
    {
        return ERR;
    }

    aml_object_t* root = aml_root_get();
    assert(root != NULL);

    // OS specific predefined objects, see section 5.7 of the ACPI specification.
    aml_object_t* osi = aml_object_new(NULL, AML_OBJECT_PREDEFINED);
    if (osi == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(osi);
    aml_method_flags_t osiFlags = {
        .argCount = 1,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(osi, osiFlags, NULL, NULL, aml_osi_implementation) == ERR ||
        aml_object_add_child(root, osi, "_OSI") == ERR)
    {
        return ERR;
    }

    aml_object_t* rev = aml_object_new(NULL, AML_OBJECT_PREDEFINED);
    if (rev == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(rev);
    aml_method_flags_t revFlags = {
        .argCount = 0,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(rev, revFlags, NULL, NULL, aml_rev_implementation) == ERR ||
        aml_object_add_child(root, rev, "_REV") == ERR)
    {
        return ERR;
    }

    aml_object_t* os = aml_object_new(NULL, AML_OBJECT_PREDEFINED);
    if (os == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(os);
    aml_method_flags_t osFlags = {
        .argCount = 0,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_method_set(os, osFlags, NULL, NULL, aml_os_implementation) == ERR ||
        aml_object_add_child(root, os, "_OS_") == ERR)
    {
        return ERR;
    }

    // TODO: Implement _GL properly.
    aml_object_t* gl = aml_object_new(NULL, AML_OBJECT_PREDEFINED);
    if (gl == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(gl);
    if (aml_mutex_set(gl, 0) == ERR || aml_object_add_child(root, gl, "_GL_") == ERR)
    {
        return ERR;
    }

    globalMutex = REF(&gl->mutex);
    return 0;
}
