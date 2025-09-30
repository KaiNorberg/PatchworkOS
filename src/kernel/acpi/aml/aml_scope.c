#include "aml_scope.h"

#include "acpi/aml/aml_node.h"
#include "acpi/aml/runtime/convert.h"

#include "log/panic.h"

uint64_t aml_scope_init(aml_scope_t* scope, aml_node_t* node)
{
    scope->node = node;
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
            aml_node_deinit(scope->temps[i]);
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
            aml_node_deinit(scope->temps[i]);
        }
    }
}

aml_node_t* aml_scope_get_temp(aml_scope_t* scope)
{
    for (uint8_t i = 0; i < AML_MAX_TEMPS; i++)
    {
        if (scope->temps[i] == NULL)
        {
            char name[5] = {'_', 'T', '_', AML_TEMP_INDEX_TO_CHAR(i), '\0'};
            scope->temps[i] = aml_node_new(NULL, name, AML_NODE_NONE);
            if (scope->temps[i] == NULL)
            {
                LOG_ERR("Failed to allocate temporary node");
                return NULL;
            }

            return scope->temps[i];
        }

        if (scope->temps[i]->type == AML_DATA_UNINITALIZED)
        {
            return scope->temps[i];
        }
    }

    LOG_ERR("Out of temporary nodes\n");
    errno = ENOMEM;
    return NULL;
}
