#pragma once

#include "aml_value.h"
#include "log/log.h"

void aml_debug_dump(aml_state_t* state);

#define AML_DEBUG_UNEXPECTED_VALUE(value) \
    LOG_ERR("unexpected value (type: %s, num: 0x%04x, name: %s) in '%s'\n", \
        aml_value_type_to_string((value)->props->type), (value)->num, (value)->props->name, __PRETTY_FUNCTION__); \
    aml_debug_dump(state)
#define AML_DEBUG_UNIMPLEMENTED_VALUE(value) \
    LOG_ERR("unimplemented value (type: %s, num: 0x%04x, name: %s) in '%s'\n", \
        aml_value_type_to_string((value)->props->type), (value)->num, (value)->props->name, __PRETTY_FUNCTION__); \
    aml_debug_dump(state)

#define AML_DEBUG_UNIMPLEMENTED_STRUCTURE(structure) \
    LOG_ERR("unimplemented structure '%s' in '%s'\n", structure, __PRETTY_FUNCTION__); \
    aml_debug_dump(state)
#define AML_DEBUG_INVALID_STRUCTURE(structure) \
    LOG_ERR("invalid structure '%s' in '%s'\n", structure, __PRETTY_FUNCTION__); \
    aml_debug_dump(state)
