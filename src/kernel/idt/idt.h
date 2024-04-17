#pragma once

#include "defs/defs.h"
#include "vectors/vectors.h"

#define IDT_INTERRUPT_GATE 0b1110
#define IDT_TRAP_GATE 0b1111

#define IDT_RING0 0b00
#define IDT_RING1 0b01
#define IDT_RING2 0b10
#define IDT_RING3 0b11

typedef struct PACKED
{
    uint16_t isrLow;      
    uint16_t codeSegment;
    uint8_t ist;
    uint8_t attributes;
    uint16_t isrMid;
    uint32_t isrHigh;
    uint32_t reserved;
} IdtEntry;

typedef struct PACKED
{
    uint16_t size;
    uint64_t offset;
} IdtDesc;

typedef struct PACKED
{
    IdtEntry entries[VECTOR_AMOUNT];
} Idt;

void idt_init(void);

void idt_load(void);