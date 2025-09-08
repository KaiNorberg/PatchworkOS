#include "aml.h"

#include "aml_state.h"
#include "term.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_state_t state = {
        .data = data,
        .dataSize = size,
        .instructionPointer = 0,
    };

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList. The DefBlockHeader is already read
    // as thats the `acpi_header_t`. So the entire code is a termlist.

    if (aml_termlist_parse(&state, size) == ERR)
    {
        LOG_ERR("failed to parse TermList\n");
        return ERR;
    }

    return 0;
}
