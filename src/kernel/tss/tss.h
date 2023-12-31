#pragma once

#include <stdint.h>

extern void tss_load();

typedef struct __attribute__((packed))
{
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb; //Also used to determine if a tss is present
} Tss;

extern void tss_load();

void tss_init();

Tss* tss_get(uint8_t cpuId);