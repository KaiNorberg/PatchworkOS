#pragma once

#include "rsdp/rsdp.h"

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint8_t Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t CheckSum;
    uint8_t OEMID[6];
    uint8_t OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision; 
} SDTHeader;

void acpi_init(XSDP* xsdp);

SDTHeader* acpi_find(const char* signature);