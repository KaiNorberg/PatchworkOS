#include <modules/acpi/aml/runtime/compare.h>

#include <modules/acpi/aml/to_string.h>
#include <kernel/log/log.h>

static inline aml_uint_t aml_compare_integers(aml_uint_t a, aml_uint_t b, aml_compare_operation_t operation)
{
    switch (operation)
    {
    case AML_COMPARE_AND:
        return (a != 0) && (b != 0) ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_EQUAL:
        return a == b ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_GREATER:
        return a > b ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_LESS:
        return a < b ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_OR:
        return (a != 0) || (b != 0) ? AML_TRUE : AML_FALSE;
    default:
        return AML_FALSE;
    }
}

aml_uint_t aml_compare_not(aml_uint_t value)
{
    return value == AML_FALSE ? AML_TRUE : AML_FALSE;
}

aml_uint_t aml_compare(aml_object_t* a, aml_object_t* b, aml_compare_operation_t operation)
{
    if (a == NULL || b == NULL)
    {
        return AML_FALSE;
    }

    if (operation >= AML_COMPARE_INVERT_BASE)
    {
        return aml_compare_not(aml_compare(a, b, operation - AML_COMPARE_INVERT_BASE));
    }

    aml_type_t aType = a->type;
    aml_type_t bType = b->type;

    if (aType != bType)
    {
        return AML_FALSE;
    }

    if (aType == AML_INTEGER)
    {
        return aml_compare_integers(a->integer.value, b->integer.value, operation);
    }

    uint64_t lenA = 0;
    uint64_t lenB = 0;
    const uint8_t* dataA = NULL;
    const uint8_t* dataB = NULL;
    switch (aType)
    {
    case AML_STRING:
    {
        lenA = a->string.length;
        lenB = b->string.length;
        dataA = (const uint8_t*)a->string.content;
        dataB = (const uint8_t*)b->string.content;
    }
    break;
    case AML_BUFFER:
    {
        lenA = a->buffer.length;
        lenB = b->buffer.length;
        dataA = a->buffer.content;
        dataB = b->buffer.content;
    }
    break;
    default:
        return AML_FALSE;
    }

    uint64_t minLen = (lenA < lenB) ? lenA : lenB;

    switch (operation)
    {
    case AML_COMPARE_EQUAL:
        if (lenA != lenB)
        {
            return AML_FALSE;
        }
        return memcmp(dataA, dataB, lenA) == 0 ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_GREATER:
        for (uint64_t i = 0; i < minLen; i++)
        {
            if (dataA[i] > dataB[i])
            {
                return AML_TRUE;
            }
            else if (dataA[i] < dataB[i])
            {
                return AML_FALSE;
            }
        }
        return lenA > lenB ? AML_TRUE : AML_FALSE;
    case AML_COMPARE_LESS:
        for (uint64_t i = 0; i < minLen; i++)
        {
            if (dataA[i] < dataB[i])
            {
                return AML_TRUE;
            }
            else if (dataA[i] > dataB[i])
            {
                return AML_FALSE;
            }
        }
        return lenA < lenB ? AML_TRUE : AML_FALSE;
    default:
        return AML_FALSE;
    }
}
