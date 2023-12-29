#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
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
} Xsdt;

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
} SdtHeader;

void rsdt_init(Xsdt* xsdp);

SdtHeader* rsdt_lookup(const char* signature);