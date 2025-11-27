#include <modules/acpi/aml/runtime/eisa_id.h>

#include <errno.h>
#include <string.h>

#define AML_EISA_ID_BYTE(c) ((uint32_t)(((c) - 0x40) & 0x1F))

uint64_t aml_eisa_id_from_string(const char* str)
{
    if (str == NULL || strnlen_s(str, 8) != 7)
    {
        errno = EINVAL;
        return ERR;
    }

    for (uint64_t i = 0; i < 3; i++)
    {
        if (str[i] < 'A' || str[i] > 'Z')
        {
            errno = EINVAL;
            return ERR;
        }
    }
    for (uint64_t i = 3; i < 7; i++)
    {
        if ((str[i] < '0' || str[i] > '9') && (str[i] < 'A' || str[i] > 'F'))
        {
            errno = EINVAL;
            return ERR;
        }
    }

    uint32_t value = 0;
    value |= (AML_EISA_ID_BYTE(str[0]) << 2) | ((AML_EISA_ID_BYTE(str[1]) >> 3) & 0x03);
    value |= ((AML_EISA_ID_BYTE(str[1]) & 0x07) << 13) | (AML_EISA_ID_BYTE(str[2]) << 8);

    const uint32_t shifts[4] = {20, 16, 28, 24};
    for (uint64_t i = 3; i < 7; i++)
    {
        uint32_t shift = shifts[i - 3];
        if (str[i] >= '0' && str[i] <= '9')
        {
            value |= (uint32_t)(str[i] - '0') << shift;
        }
        else
        {
            value |= (uint32_t)(str[i] - 'A' + 10) << shift;
        }
    }

    return value;
}

#define AML_EISA_ID_CHAR(b) ((char)(((b) & 0x1F) + 0x40))

uint64_t aml_eisa_id_to_string(uint32_t eisaId, char* buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize < 8)
    {
        errno = EINVAL;
        return ERR;
    }

    buffer[0] = AML_EISA_ID_CHAR((eisaId >> 2) & 0x1F);
    buffer[1] = AML_EISA_ID_CHAR(((eisaId >> 13) & 0x07) | ((eisaId & 0x03) << 3));
    buffer[2] = AML_EISA_ID_CHAR((eisaId >> 8) & 0x1F);

    const uint32_t shifts[4] = {20, 16, 28, 24};
    for (uint64_t i = 3; i < 7; i++)
    {
        uint32_t shift = shifts[i - 3];
        uint8_t nibble = (eisaId >> shift) & 0x0F;
        if (nibble < 10)
        {
            buffer[i] = (char)('0' + nibble);
        }
        else
        {
            buffer[i] = (char)('A' + (nibble - 10));
        }
    }
    buffer[7] = '\0';

    return 0;
}