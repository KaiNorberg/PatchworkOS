#include "lock_rule.h"

#include "acpi/aml/aml_object.h"

bool aml_should_acquire_global_mutex(aml_object_t* object)
{
    if (object == NULL)
    {
        return false;
    }

    switch (object->type)
    {
    case AML_DATA_FIELD_UNIT:
        return object->fieldUnit.flags.lockRule == AML_LOCK_RULE_LOCK;
    default:
        return false;
    }
}
