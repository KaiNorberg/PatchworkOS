#include "aml_scope.h"

#include "acpi/aml/aml_object.h"
#include "log/log.h"
#include "mem/heap.h"

uint64_t aml_scope_init(aml_scope_t* scope, aml_object_t* location)
{
    scope->location = location;
    scope->temps = NULL;
    scope->tempCount = 0;

    return 0;
}

void aml_scope_deinit(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < scope->tempCount; i++)
    {
        aml_object_deinit(&scope->temps[i]);
    }
    heap_free(scope->temps);
    scope->temps = NULL;
    scope->tempCount = 0;
}

void aml_scope_reset_temps(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < scope->tempCount; i++)
    {
        aml_object_deinit(&scope->temps[i]);
    }
}

aml_object_t* aml_scope_get_temp(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < scope->tempCount; i++)
    {
        if (scope->temps[i].type == AML_DATA_UNINITALIZED)
        {
            return &scope->temps[i];
        }
    }

    uint64_t newTempCount = scope->tempCount + AML_SCOPE_TEMP_STEP;
    void* newTemps = heap_realloc(scope->temps, sizeof(aml_object_t) * newTempCount, HEAP_NONE);
    if (newTemps == NULL)
    {
        return NULL;
    }
    scope->temps = newTemps;
    for (uint8_t i = scope->tempCount; i < newTempCount; i++)
    {
        scope->temps[i] = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        scope->temps[i].segment[0] = '_';
        scope->temps[i].segment[1] = 'T';
        scope->temps[i].segment[2] = '_';
        scope->temps[i].segment[3] = 'T';
    }
    scope->tempCount = newTempCount;

    return &scope->temps[scope->tempCount - AML_SCOPE_TEMP_STEP];
}
