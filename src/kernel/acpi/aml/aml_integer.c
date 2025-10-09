#include "aml_integer.h"

#include "acpi/tables.h"
#include "log/log.h"

static uint8_t integerByteSize = 0;

uint64_t aml_integer_handling_init(void)
{
    dsdt_t* dsdt = DSDT_GET();
    if (dsdt == NULL)
    {
        LOG_ERR("failed to retrieve DSDT\n");
        return ERR;
    }
    integerByteSize = dsdt->header.revision < 2 ? 4 : 8; // Section 5.2.11.1

    LOG_INFO("using AML integer size %u bits\n", integerByteSize * 8);
    return 0;
}

uint8_t aml_integer_byte_size(void)
{
    return integerByteSize;
}

uint8_t aml_integer_bit_size(void)
{
    return integerByteSize * 8;
}

aml_integer_t aml_integer_ones(void)
{
    return integerByteSize == 4 ? UINT32_MAX : UINT64_MAX;
}
