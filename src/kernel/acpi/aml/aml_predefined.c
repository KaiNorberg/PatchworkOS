#include "aml_predefined.h"

#include "aml.h"
#include "aml_object.h"
#include "aml_to_string.h"
#include "log/log.h"

#include <errno.h>

static uint64_t aml_predefined_osi_implementation(aml_object_t* method, uint64_t argCount, aml_object_t** args,
    aml_object_t* returnValue)
{
    // See section 5.7.2 of the ACPI specification.
    (void)method; // Unused

    if (argCount != 1 || args[0]->type != AML_DATA_STRING)
    {
        errno = EINVAL;
        return ERR;
    }

    LOG_DEBUG("_OSI called with argument: '%.*s'\n", (int)args[0]->string.length, args[0]->string.content);

    // TODO: Implement this properly. For now we just return true for everything.
    if (aml_object_init_integer(returnValue, UINT64_MAX) == ERR)
    {
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_create_predefined_device(const char* name)
{
    aml_object_t* root = aml_root_get();
    assert(root != NULL);

    aml_object_t* object = aml_object_new(root, name, AML_OBJECT_PREDEFINED);
    if (object == NULL)
    {
        return ERR;
    }

    if (aml_object_init_device(object) == ERR)
    {
        aml_object_free(object);
        return ERR;
    }

    return 0;
}

uint64_t aml_predefined_init(void)
{
    // Normal predefined root objects, see section 5.3.1 of the ACPI specification.
    if (aml_create_predefined_device("_GPE") == ERR || aml_create_predefined_device("_PR") == ERR ||
        aml_create_predefined_device("_SB") == ERR || aml_create_predefined_device("_SI") == ERR ||
        aml_create_predefined_device("_TZ") == ERR)
    {
        return ERR;
    }

    aml_object_t* root = aml_root_get();
    assert(root != NULL);

    // OS specific predefined objects, see section 5.7 of the ACPI specification.
    aml_object_t* osi = aml_object_new(root, "_OSI", AML_OBJECT_PREDEFINED);
    if (osi == NULL)
    {
        return ERR;
    }

    aml_method_flags_t osiFlags = {
        .argCount = 1,
        .isSerialized = true,
        .syncLevel = 15,
    };
    if (aml_object_init_method(osi, &osiFlags, NULL, NULL, aml_predefined_osi_implementation) == ERR)
    {
        aml_object_free(osi);
        return ERR;
    }

    // TODO: Implement _OS, _GL and _REV.

    return 0;
}
