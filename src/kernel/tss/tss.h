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
    uint16_t iopb;
} TaskStateSegment;

extern TaskStateSegment* tss;

void tss_init();