#include <kernel/acpi/aml/state.h>

#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/copy.h>



status_t aml_state_init(aml_state_t* state, aml_object_t** args)
{
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        state->locals[i] = NULL;
    }

    uint8_t argIndex = 0;
    if (args != NULL)
    {
        while (args[argIndex] != NULL)
        {
            if (argIndex >= AML_MAX_ARGS)
            {
                for (uint8_t j = 0; j < AML_MAX_LOCALS; j++)
                {
                    UNREF(state->locals[j]);
                }
                for (uint8_t k = 0; k < argIndex; k++)
                {
                    UNREF(state->args[k]);
                }
                return ERR(ACPI, TOOBIG);
            }

            aml_object_t* arg = aml_object_new();
            if (arg == NULL)
            {
                for (uint8_t j = 0; j < AML_MAX_LOCALS; j++)
                {
                    UNREF(state->locals[j]);
                }
                for (uint8_t k = 0; k < argIndex; k++)
                {
                    UNREF(state->args[k]);
                }
                return ERR(MEM, NOMEM);
            }

            status_t status = aml_arg_set(arg, args[argIndex]);
            if (IS_ERR(status))
            {
                UNREF(arg);
                for (uint8_t j = 0; j < AML_MAX_LOCALS; j++)
                {
                    UNREF(state->locals[j]);
                }
                for (uint8_t k = 0; k < argIndex; k++)
                {
                    UNREF(state->args[k]);
                }
                return status;
            }

            state->args[argIndex] = &arg->arg;
            state->args[argIndex]->name = AML_NAME('A', 'R', 'G', '0' + argIndex);
            argIndex++;
        }
    }
    for (; argIndex < AML_MAX_ARGS; argIndex++)
    {
        state->args[argIndex] = NULL;
    }

    state->result = NULL;
    state->errorDepth = 0;
    aml_overlay_init(&state->overlay);
    return OK;
}

void aml_state_deinit(aml_state_t* state)
{
    for (uint8_t i = 0; i < AML_MAX_LOCALS; i++)
    {
        UNREF(state->locals[i]);
    }
    for (uint8_t i = 0; i < AML_MAX_ARGS; i++)
    {
        UNREF(state->args[i]);
    }
    if (state->result != NULL)
    {
        UNREF(state->result);
    }
    state->result = NULL;

    aml_overlay_deinit(&state->overlay);
}

status_t aml_state_result_get(aml_state_t* state, aml_object_t** out)
{
    if (state == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    if (state->result == NULL)
    {
        // The method never had any expressions evaluated or explicitly returned a value.
        status_t status = aml_integer_set(result, 0);
        if (IS_ERR(status))
        {
            UNREF(result);
            return status;
        }
        result->flags |= AML_OBJECT_EXCEPTION_ON_USE;
        *out = result;
        return OK;
    }

    status_t status = aml_copy_object(state, state->result, result);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }
    *out = result;
    return OK;
}

void aml_state_result_set(aml_state_t* state, aml_object_t* result)
{
    if (state == NULL)
    {
        return;
    }

    if (state->result != NULL)
    {
        UNREF(state->result);
        state->result = NULL;
    }

    if (result != NULL)
    {
        state->result = REF(result);
    }
}
