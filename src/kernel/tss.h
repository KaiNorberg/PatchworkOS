#pragma once

#include "defs.h"

typedef struct PACKED
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
} Tss;

extern void tss_load(void);

void tss_init(Tss* tss);

void tss_stack_load(Tss* tss, void* stackTop);