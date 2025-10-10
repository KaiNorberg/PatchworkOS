#include "aml_state.h"

#include "aml_object.h"
#include "log/log.h"
#include "runtime/copy.h"

uint64_t aml_state_init(aml_state_t* state, aml_object_t** args,
    uint64_t argCount)
{
    // We give the locals and args names so they are identifiable when debugging but these names dont do anything.

    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        aml_object_t* local = aml_object_new(NULL);
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
        aml_object_t* arg = aml_object_new(NULL);
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
    state->result = NULL;
    state->errorDepth = 0;
    list_init(&state->createdObjects);
    return 0;
}

void aml_state_deinit(aml_state_t* state)
{
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        DEREF(state->locals[i]);
    }
    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        DEREF(state->args[i]);
    }
    if (state->result != NULL)
    {
        DEREF(state->result);
    }
    state->result = NULL;

    while (!list_is_empty(&state->createdObjects))
    {
        aml_object_t* child = CONTAINER_OF(list_pop(&state->createdObjects), aml_object_t, stateEntry);
        // Dont do anything.
    }
}

void aml_state_garbage_collect(aml_state_t* state)
{
    while (!list_is_empty(&state->createdObjects))
    {
        aml_object_t* child = CONTAINER_OF(list_pop(&state->createdObjects), aml_object_t, stateEntry);
        aml_object_remove(child);
    }
}

aml_object_t* aml_state_result_get(aml_state_t* state)
{
    if (state == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (state->result == NULL)
    {
        aml_object_t* result = aml_object_new(NULL);
        if (result == NULL)
        {
            return NULL;
        }

        if (aml_integer_set(result, 0) == ERR)
        {
            DEREF(result);
            return NULL;
        }

        return result;
    }

    return REF(state->result);
}

void aml_state_result_set(aml_state_t* state, aml_object_t* result)
{
    if (state == NULL)
    {
        return;
    }

    if (state->result != NULL)
    {
        DEREF(state->result);
        state->result = NULL;
    }

    if (result != NULL)
    {
        state->result = REF(result);
    }
}
