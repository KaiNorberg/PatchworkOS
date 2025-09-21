#include "object_reference.h"

#include <errno.h>

uint64_t aml_object_reference_init(aml_object_reference_t* ref, aml_node_t* node)
{
    ref->node = node;
    return 0;
}

void aml_object_reference_deinit(aml_object_reference_t* ref)
{
    if (ref == NULL)
    {
        return;
    }

    ref->node = NULL;
}

bool aml_object_reference_is_null(aml_object_reference_t* ref)
{
    return ref == NULL || ref->node == NULL;
}

aml_node_t* aml_object_reference_deref(aml_object_reference_t* ref)
{
    if (aml_object_reference_is_null(ref))
    {
        return NULL;
    }

    return ref->node;
}
