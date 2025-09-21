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
    case AML_NODE_FIELD:
        return node->field.flags.lockRule == AML_LOCK_RULE_LOCK;
    case AML_NODE_INDEX_FIELD:
        return node->indexField.flags.lockRule == AML_LOCK_RULE_LOCK;
    case AML_NODE_BANK_FIELD:
        return node->bankField.flags.lockRule == AML_LOCK_RULE_LOCK;
    default:
        return false;
    }
}
