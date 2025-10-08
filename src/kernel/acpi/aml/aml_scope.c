#include "aml_scope.h"

#include "acpi/aml/aml_object.h"
#include "log/log.h"
#include "mem/heap.h"

uint64_t aml_scope_init(aml_scope_t* scope, aml_object_t* location)
{
    scope->location = REF(location);
    return 0;
}

void aml_scope_deinit(aml_scope_t* scope)
{
    DEREF(scope->location);
    scope->location = NULL;
}
