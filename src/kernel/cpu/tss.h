#pragma once

#include <common/defs.h>

#include <stdint.h>

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
} tss_t;

extern void tss_load(void);

void tss_init(tss_t* tss);

void tss_stack_load(tss_t* tss, void* stackTop);
