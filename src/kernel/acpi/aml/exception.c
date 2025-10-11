#include "exception.h"

#include "log/log.h"
#include "mem/heap.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    aml_exception_t code;
    const char* name;
} aml_exception_info_t;

static const aml_exception_info_t exceptionTable[] = {
    {AML_ERROR, "AE_AML_ERROR"},
    {AML_PARSE, "AE_AML_PARSE"},
    {AML_BAD_OPCODE, "AE_AML_BAD_OPCODE"},
    {AML_NO_OPERAND, "AE_AML_NO_OPERAND"},
    {AML_OPERAND_TYPE, "AE_AML_OPERAND_TYPE"},
    {AML_OPERAND_VALUE, "AE_AML_OPERAND_VALUE"},
    {AML_UNINITIALIZED_LOCAL, "AE_AML_UNINITIALIZED_LOCAL"},
    {AML_UNINITIALIZED_ARG, "AE_AML_UNINITIALIZED_ARG"},
    {AML_UNINITIALIZED_ELEMENT, "AE_AML_UNINITIALIZED_ELEMENT"},
    {AML_NUMERIC_OVERFLOW, "AE_AML_NUMERIC_OVERFLOW"},
    {AML_REGION_LIMIT, "AE_AML_REGION_LIMIT"},
    {AML_BUFFER_LIMIT, "AE_AML_BUFFER_LIMIT"},
    {AML_PACKAGE_LIMIT, "AE_AML_PACKAGE_LIMIT"},
    {AML_DIVIDE_BY_ZERO, "AE_AML_DIVIDE_BY_ZERO"},
    {AML_BAD_NAME, "AE_AML_BAD_NAME"},
    {AML_NAME_NOT_FOUND, "AE_AML_NAME_NOT_FOUND"},
    {AML_INTERNAL, "AE_AML_INTERNAL"},
    {AML_INVALID_SPACE_ID, "AE_AML_INVALID_SPACE_ID"},
    {AML_STRING_LIMIT, "AE_AML_STRING_LIMIT"},
    {AML_NO_RETURN_VALUE, "AE_AML_NO_RETURN_VALUE"},
    {AML_METHOD_LIMIT, "AE_AML_METHOD_LIMIT"},
    {AML_NOT_OWNER, "AE_AML_NOT_OWNER"},
    {AML_MUTEX_ORDER, "AE_AML_MUTEX_ORDER"},
    {AML_MUTEX_NOT_ACQUIRED, "AE_AML_MUTEX_NOT_ACQUIRED"},
    {AML_INVALID_RESOURCE_TYPE, "AE_AML_INVALID_RESOURCE_TYPE"},
    {AML_INVALID_INDEX, "AE_AML_INVALID_INDEX"},
    {AML_REGISTER_LIMIT, "AE_AML_REGISTER_LIMIT"},
    {AML_NO_WHILE, "AE_AML_NO_WHILE"},
    {AML_ALIGNMENT, "AE_AML_ALIGNMENT"},
    {AML_NO_RESOURCE_END_TAG, "AE_AML_NO_RESOURCE_END_TAG"},
    {AML_BAD_RESOURCE_VALUE, "AE_AML_BAD_RESOURCE_VALUE"},
    {AML_CIRCULAR_REFERENCE, "AE_AML_CIRCULAR_REFERENCE"},
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

    aml_exception_handler_t* newHandlers = heap_alloc(sizeof(aml_exception_handler_t) * (handlerCount + 1), HEAP_NONE);
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
        heap_free(handlers);
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
        heap_free(handlers);
        handlers = NULL;
        return;
    }
}

void aml_exception_raise(aml_exception_t code, const char* function)
{
#ifdef DEBUG_TESTING
    LOG_WARN("AML EXCEPTION '%s' (0x%x) in '%s()'. (In testing mode so probably intentional)\n",
        aml_exception_to_string(code), code, function);
#else
    LOG_WARN("AML EXCEPTION '%s' (0x%x) in '%s()'\n", aml_exception_to_string(code), code, function);
#endif

    for (uint64_t i = 0; i < handlerCount; i++)
    {
        handlers[i](code);
    }
}
