#include "aml_node.h"

const char* aml_node_type_to_string(aml_node_type_t type)
{
    switch (type)
    {
    case AML_NODE_NONE:
        return "None";
    case AML_NODE_PREDEFINED:
        return "Predefined";
    case AML_NODE_DEVICE:
        return "Device";
    case AML_NODE_PROCESSOR:
        return "Processor";
    case AML_NODE_THERMAL_ZONE:
        return "ThermalZone";
    case AML_NODE_POWER_RESOURCE:
        return "PowerResource";
    case AML_NODE_OPREGION:
        return "OpRegion";
    default:
        return "Unknown";
    }
}
