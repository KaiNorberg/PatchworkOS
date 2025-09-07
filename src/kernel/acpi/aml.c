#include "aml.h"

#include "aml_handlers.h"
#include "aml_state.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>

static aml_op_t operations[] = {
    [0x10] = {.name = "ScopeOp", .handler = aml_handler_scope_op},
};

#define OPERATION_COUNT (sizeof(operations) / sizeof(operations[0]))

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_state_t state = {.instructionPointer = 0, .data = data, .dataSize = size};

    uint8_t opcode;
    while (aml_state_read_byte(&state, &opcode) != 0)
    {
        if (opcode >= OPERATION_COUNT)
        {
            LOG_ERR("Unknown opcode: 0x%.2x\n", opcode);
            errno = EINVAL;
            return ERR;
        }
        const aml_op_t* op = &operations[opcode];
        if (op == NULL)
        {
            LOG_ERR("Unknown opcode: 0x%.2x\n", opcode);
            errno = EINVAL;
            return ERR;
        }

        if (op->handler(&state) == ERR)
        {
            LOG_ERR("Failed to handle opcode: 0x%.2x\n", opcode);
            return ERR;
        }
    }

    return 0;
}
