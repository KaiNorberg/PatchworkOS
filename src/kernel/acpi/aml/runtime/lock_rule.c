#include "lock_rule.h"

#include "acpi/aml/aml_node.h"

bool aml_should_acquire_global_mutex(aml_node_t* node)
{
    if (node == NULL)
    {
        return false;
    }

    switch (node->type)
    {
    case AML_DATA_FIELD_UNIT:
        return node->fieldUnit.flags.lockRule == AML_LOCK_RULE_LOCK;
    default:
        return false;
    }
}
