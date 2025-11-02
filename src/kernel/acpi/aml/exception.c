#include <kernel/acpi/aml/exception.h>

#include <kernel/log/log.h>
#include <kernel/sched/thread.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    aml_exception_t code;
    const char* name;
} aml_exception_info_t;

static const aml_exception_info_t exceptionTable[] = {
    {AML_NOT_FOUND, "AE_NOT_FOUND"},
    {AML_ERROR, "AE_AML_ERROR"},
    {AML_PARSE, "AE_AML_PARSE"},
    {AML_DIVIDE_BY_ZERO, "AE_AML_DIVIDE_BY_ZERO"},
};

static aml_exception_handler_t* handlers = NULL;
static uint64_t handlerCount = 0;

const char* aml_exception_to_string(aml_exception_t code)
{
    for (uint64_t i = 0; i < sizeof(exceptionTable) / sizeof(exceptionTable[0]); i++)
    {
        if (exceptionTable[i].code == code)
        {
            return exceptionTable[i].name;
        }
    }
    return "AE_AML_UNKNOWN_EXCEPTION";
}

uint64_t aml_exception_register(aml_exception_handler_t handler)
{
    for (uint64_t i = 0; i < handlerCount; i++)
    {
        if (handlers[i] == handler)
        {
            errno = EEXIST;
            return ERR;
        }
    }

    aml_exception_handler_t* newHandlers = malloc(sizeof(aml_exception_handler_t) * (handlerCount + 1));
    if (newHandlers == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < handlerCount; i++)
    {
        newHandlers[i] = handlers[i];
    }
    newHandlers[handlerCount] = handler;
    handlerCount++;

    if (handlers != NULL)
    {
        free(handlers);
    }
    handlers = newHandlers;

    return 0;
}

void aml_exception_unregister(aml_exception_handler_t handler)
{
    if (handlers == NULL || handlerCount == 0)
    {
        return;
    }

    uint64_t index = handlerCount;
    for (uint64_t i = 0; i < handlerCount; i++)
    {
        if (handlers[i] == handler)
        {
            index = i;
            break;
        }
    }

    if (index == handlerCount)
    {
        return;
    }

    // Perhaps slightly inefficient, but whatever.
    memmove(&handlers[index], &handlers[index + 1], sizeof(aml_exception_handler_t) * (handlerCount - index - 1));
    handlerCount--;

    if (handlerCount == 0)
    {
        free(handlers);
        handlers = NULL;
        return;
    }
}

void aml_exception_raise(aml_state_t* state, aml_exception_t code, const char* function)
{
#ifdef TESTING
    LOG_WARN("AML EXCEPTION '%s' (0x%x) in '%s()'. (Probably intentional)\n", aml_exception_to_string(code), code,
        function);
#else
    LOG_WARN("AML EXCEPTION '%s' (0x%x) in '%s()'\n", aml_exception_to_string(code), code, function);
#endif

    for (uint64_t i = 0; i < handlerCount; i++)
    {
        handlers[i](state, code);
    }
}
