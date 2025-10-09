#include "aml_state.h"

#include "aml_object.h"
#include "log/log.h"
#include "runtime/copy.h"

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

    // We give the locals and args names so they are identifiable when debugging but these names dont do anything.

    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        aml_object_t* local = aml_object_new(NULL, AML_OBJECT_NONE);
        if (local == NULL || aml_local_set(local) == ERR)
        {
            if (local != NULL)
            {
                DEREF(local);
            }
            for (uint8_t j = 0; j < i; j++)
            {
                DEREF(state->locals[j]);
            }
            return ERR;
        }
        state->locals[i] = &local->local;

        state->locals[i]->name.segment[0] = 'L';
        state->locals[i]->name.segment[1] = 'O';
        state->locals[i]->name.segment[2] = 'C';
        state->locals[i]->name.segment[3] = '0' + i;
        state->locals[i]->name.segment[4] = '\0';
    }
    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        aml_object_t* arg = aml_object_new(NULL, AML_OBJECT_NONE);
        if (arg == NULL || aml_arg_set(arg, argCount > i ? args[i] : NULL) == ERR)
        {
            if (arg != NULL)
            {
                DEREF(arg);
            }
            for (uint8_t j = 0; j < i; j++)
            {
                DEREF(state->args[j]);
            }
            for (uint8_t j = 0; j < AML_MAX_LOCALS; j++)
            {
                DEREF(state->locals[j]);
            }
            return ERR;
        }
        state->args[i] = &arg->arg;

        state->args[i]->name.segment[0] = 'A';
        state->args[i]->name.segment[1] = 'R';
        state->args[i]->name.segment[2] = 'G';
        state->args[i]->name.segment[3] = '0' + i;
        state->args[i]->name.segment[4] = '\0';
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
