#include "aml_scope.h"

#include "acpi/aml/aml_object.h"
#include "log/log.h"
#include "mem/heap.h"

uint64_t aml_scope_init(aml_scope_t* scope, aml_object_t* location)
{
    scope->location = REF(location);
    scope->temps = NULL;
    scope->tempCount = 0;

    return 0;
}

void aml_scope_deinit(aml_scope_t* scope)
{
    DEREF(scope->location);
    scope->location = NULL;
    for (uint64_t i = 0; i < scope->tempCount; i++)
    {
        if (atomic_load(&scope->temps[i]->ref.count) != 1)
        {
            LOG_WARN("Temporary object %llu still has references, possible memory leak\n", i);
        }
        DEREF(scope->temps[i]);
    }
    heap_free(scope->temps);
    scope->temps = NULL;
    scope->tempCount = 0;
}

void aml_scope_reset_temps(aml_scope_t* scope)
{
    for (uint64_t i = 0; i < scope->tempCount; i++)
    {
        aml_object_deinit(scope->temps[i]);
    }
}

aml_object_t* aml_scope_get_temp(aml_scope_t* scope)
{
    for (uint64_t i = 0; i < scope->tempCount; i++)
    {
        if (scope->temps[i]->type == AML_UNINITIALIZED)
        {
            return scope->temps[i];
        }
    }

    uint64_t newCount = scope->tempCount + AML_SCOPE_TEMP_STEP;
    aml_object_t** newTemps = heap_alloc(sizeof(aml_object_t*) * newCount, HEAP_NONE);
    if (newTemps == NULL)
    {
        return NULL;
    }

    for (uint64_t i = 0; i < scope->tempCount; i++)
    {
        newTemps[i] = scope->temps[i];
    }
    for (uint64_t i = scope->tempCount; i < newCount; i++)
    {
        newTemps[i] = aml_object_new(NULL, AML_OBJECT_NONE);
        if (newTemps[i] == NULL)
        {
            for (uint64_t j = scope->tempCount; j < i; j++)
            {
                DEREF(newTemps[j]);
            }
            heap_free(newTemps);
            return NULL;
        }
    }

    heap_free(scope->temps);
    scope->temps = newTemps;
    scope->tempCount = newCount;
    return scope->temps[scope->tempCount - AML_SCOPE_TEMP_STEP];
}
