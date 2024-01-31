#include "idt.h"

#include "io/io.h"
#include "tty/tty.h"

#include "page_allocator/page_allocator.h"
#include "global_heap/global_heap.h"

void idt_load(Idt* idt)
{
    IdtDesc idtDesc;
    idtDesc.size = (sizeof(Idt)) - 1;
    idtDesc.offset = (uint64_t)idt;
    idt_load_descriptor(&idtDesc);
}

void idt_set_vector(Idt* idt, uint8_t vector, void* isr, uint8_t privilageLevel, uint8_t gateType)
{
    IdtEntry* descriptor = &(idt->entries[vector]);
 
    descriptor->isrLow = (uint64_t)isr & 0xFFFF;
    descriptor->codeSegment = 0x08;
    descriptor->ist = 0;
    descriptor->attributes = 0b10000000 | (uint8_t)(privilageLevel << 5) | gateType;
    descriptor->isrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}