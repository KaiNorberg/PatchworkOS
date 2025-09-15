#include "aml_debug.h"

#include "aml_state.h"

#include "log/log.h"

void aml_debug_dump(aml_state_t* state)
{
    // Dump the region around the current positon

    uint64_t start = (state->pos >= 16) ? state->pos - 16 : 0;
    uint64_t end = (state->pos + 16 < state->dataSize) ? state->pos + 16 : state->dataSize - 1;

    LOG_WARN("==AML Dump (pos=0x%lx)==\n", state->pos);
    for (uint64_t i = start; i <= end; i += 16)
    {
        LOG_WARN("%08x: ", i);
        for (uint64_t j = 0; j < 16; j++)
        {
            if (i + j <= end)
            {
                LOG_WARN("%02x ", ((uint8_t*)state->data)[i + j]);
            }
            else
            {
                LOG_WARN("   ");
            }
        }
        LOG_WARN(" | ");
        for (uint64_t j = 0; j < 16; j++)
        {
            if (i + j <= end)
            {
                uint8_t c = ((uint8_t*)state->data)[i + j];
                if (c >= 32 && c <= 126)
                {
                    LOG_WARN("%c", c);
                }
                else
                {
                    LOG_WARN(".");
                }
            }
        }
        LOG_WARN("\n");
    }
}
