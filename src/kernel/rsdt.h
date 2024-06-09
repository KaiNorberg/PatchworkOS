#pragma once

#include "defs.h"

#define ACPI_REVISION_1_0 0
#define ACPI_REVISION_2_0 2

typedef struct PACKED
{
    char signature[8];
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtAddress;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} Xsdp;

typedef struct PACKED
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
} SdtHeader;

typedef struct PACKED
{
    SdtHeader header;
    SdtHeader* tables[];
} Xsdt;

void rsdt_init(Xsdp* xsdp);

SdtHeader* rsdt_lookup(const char* signature);