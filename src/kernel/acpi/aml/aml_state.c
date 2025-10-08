#include "aml_state.h"

#include "aml_object.h"
#include "log/log.h"
#include "log/panic.h"

uint64_t aml_state_init(aml_state_t* state, const uint8_t* start, const uint8_t* end, uint64_t argCount,
    aml_object_t** args, aml_object_t* returnValue)
{
    if (start > end)
    {
        return ERR;
    }

    state->start = start;
    state->end = end;
    state->current = start;
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        state->locals[i] = aml_object_new(NULL, AML_OBJECT_LOCAL);
        if (state->locals[i] == NULL)
        {
            for (uint8_t j = 0; j < i; j++)
            {
                DEREF(state->locals[j]);
            }
            return ERR;
        }
    }
    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        state->args[i] = aml_object_new(NULL, AML_OBJECT_ARG);
        if (state->args[i] == NULL)
        {
            for (uint8_t j = 0; j < AML_MAX_LOCALS; j++)
            {
                DEREF(state->locals[j]);
            }
            for (uint8_t j = 0; j < i; j++)
            {
                DEREF(state->args[j]);
            }
            return ERR;
        }
    }
    for (uint8_t i = 0; i < argCount && i < AML_MAX_ARGS; i++)
    {
        // Im honestly not sure how arguments are supposed to be passed. The spec seems a bit vague or ive missed
        // something. But my interpretation is that arguments should always be ObjectReferences.
        if (aml_object_reference_init(state->args[i], args[i]) == ERR)
        {
            aml_state_deinit(state);
            return ERR;
        }
    }
    state->returnValue = returnValue;
    state->lastErrPos = NULL;
    state->errorDepth = 0;
    state->flowControl = AML_FLOW_CONTROL_EXECUTE;
    list_init(&state->createdObjects);
    return 0;
}

uint64_t aml_state_deinit(aml_state_t* state)
{
    state->start = NULL;
    state->end = NULL;
    state->current = NULL;

    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        DEREF(state->locals[i]);
    }
    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        DEREF(state->args[i]);
    }
    state->returnValue = NULL;
    state->lastErrPos = NULL;

    if (state->flowControl != AML_FLOW_CONTROL_EXECUTE && state->flowControl != AML_FLOW_CONTROL_RETURN)
    {
        LOG_ERR("AML state deinitalized with invalid flow control state %d, possibly tried to Break or Continue "
                "outside of While loop\n",
            state->flowControl);
        errno = EBUSY;
        return ERR;
    }

    while (!list_is_empty(&state->createdObjects))
    {
        aml_object_t* child = CONTAINER_OF(list_pop(&state->createdObjects), aml_object_t, stateEntry);
        // Dont do anything.
    }

    return 0;
}

void aml_state_garbage_collect(aml_state_t* state)
{
    while (!list_is_empty(&state->createdObjects))
    {
        aml_object_t* child = CONTAINER_OF(list_pop(&state->createdObjects), aml_object_t, stateEntry);
        aml_object_remove(child);
    }
}
