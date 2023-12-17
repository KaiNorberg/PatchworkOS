#pragma once

#include "rsdp/rsdp.h"

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checkSum;
    uint8_t oemId[6];
    uint8_t oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorID;
    uint32_t creatorRevision; 
} SDTHeader;

void acpi_init(XSDP* xsdp);

SDTHeader* acpi_find(const char* signature);