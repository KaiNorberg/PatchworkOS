#include "data.h"

uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out)
{
    uint64_t byte = aml_state_byte_read(state);
    if (byte == ERR)
    {
        return ERR;
    }
    *out = byte;
    return 0;
}
