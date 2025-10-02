#include "aml_state.h"

#include "log/log.h"
#include "log/panic.h"

uint64_t aml_state_init(aml_state_t* state, const uint8_t* start, const uint8_t* end, aml_term_arg_list_t* args,
    aml_object_t* returnValue)
{
    if (start > end)
    {
        LOG_ERR("Invalid AML data start > end\n");
        return ERR;
    }

    state->start = start;
    state->end = end;
    state->current = start;
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        state->locals[i] = AML_OBJECT_CREATE(AML_OBJECT_LOCAL);
    }
    state->args = args;
    state->returnValue = returnValue;
    state->lastErrPos = NULL;
    state->errorDepth = 0;
    state->flowControl = AML_FLOW_CONTROL_EXECUTE;
    if (aml_mutex_stack_init(&state->mutexStack) == ERR)
    {
        LOG_ERR("Failed to initialize AML mutex stack\n");
        return ERR;
    }
    return 0;
}

uint64_t aml_state_deinit(aml_state_t* state)
{
    state->start = NULL;
    state->end = NULL;
    state->current = NULL;

    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        aml_object_deinit(&state->locals[i]);
    }
    state->args = NULL;
    state->returnValue = NULL;
    state->lastErrPos = NULL;

    if (state->mutexStack.acquiredMutexCount != 0)
    {
        aml_mutex_stack_deinit(&state->mutexStack);
        LOG_ERR("AML state deinitalized while still holding %llu mutexes\n", state->mutexStack.acquiredMutexCount);
        errno = EBUSY;
        return ERR;
    }
    aml_mutex_stack_deinit(&state->mutexStack);

    if (state->flowControl != AML_FLOW_CONTROL_EXECUTE && state->flowControl != AML_FLOW_CONTROL_RETURN)
    {
        LOG_ERR("AML state deinitalized with invalid flow control state %d, possibly tried to Break or Continue "
                "outside of While loop\n",
            state->flowControl);
        errno = EBUSY;
        return ERR;
    }

    return 0;
}
