#include "aml_scope.h"

#include "acpi/aml/aml_object.h"
#include "log/log.h"
#include "mem/heap.h"

uint64_t aml_scope_init(aml_scope_t* scope, aml_object_t* location)
{
    scope->location = location;
    for (uint8_t i = 0; i < AML_MAX_TEMPS; i++)
    {
        scope->temps[i] = NULL;
    }

    return 0;
}

void aml_scope_deinit(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < AML_MAX_TEMPS; i++)
    {
        if (scope->temps[i] != NULL)
        {
            aml_object_deinit(scope->temps[i]);
            heap_free(scope->temps[i]);
            scope->temps[i] = NULL;
        }
    }
}

void aml_scope_reset_temps(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < AML_MAX_TEMPS; i++)
    {
        if (scope->temps[i] != NULL)
        {
            aml_object_deinit(scope->temps[i]);
        }
    }
}

aml_object_t* aml_scope_get_temp(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < AML_MAX_TEMPS; i++)
    {
        if (scope->temps[i] == NULL)
        {
            char name[5] = {'_', 'T', '_', AML_TEMP_INDEX_TO_CHAR(i), '\0'};
            scope->temps[i] = aml_object_new(NULL, name, AML_OBJECT_NONE);
            if (scope->temps[i] == NULL)
            {
                LOG_ERR("Failed to allocate temporary object");
                return NULL;
            }

            return scope->temps[i];
        }

        if (scope->temps[i]->type == AML_DATA_UNINITALIZED)
        {
            return scope->temps[i];
        }
    }

    LOG_ERR("Out of temporary objects\n");
    errno = ENOMEM;
    return NULL;
}
