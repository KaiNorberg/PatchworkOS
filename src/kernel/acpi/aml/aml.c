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

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList. The DefBlockHeader is already read
    // as thats the `acpi_header_t`. So the entire code is a termlist.

    aml_state_t state;
    aml_state_init(&state, data, size);

    uint64_t result = aml_termlist_read(&state, size);

    aml_state_deinit(&state);
    return result;
}
