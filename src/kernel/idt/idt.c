#include "idt.h"

#include "syscall/syscall.h"

__attribute__((aligned(0x1000)))
static Idt idt;

extern void idt_load_descriptor(IdtDesc* descriptor);

static inline void idt_set_vector(uint8_t vector, void* isr, uint8_t privilegeLevel, uint8_t gateType)
{
    IdtEntry* descriptor = &(idt.entries[vector]);
 
    descriptor->isrLow = (uint64_t)isr & 0xFFFF;
    descriptor->codeSegment = 0x08;
    descriptor->ist = 0;
    descriptor->attributes = 0b10000000 | (uint8_t)(privilegeLevel << 5) | gateType;
    descriptor->isrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}

void idt_init()
{
    for (uint16_t vector = 0; vector < VECTOR_AMOUNT; vector++) 
    {        
        idt_set_vector((uint8_t)vector, vectorTable[vector], IDT_RING0, IDT_INTERRUPT_GATE);
    }        
    
    idt_set_vector(SYSCALL_VECTOR, vectorTable[SYSCALL_VECTOR], IDT_RING3, IDT_TRAP_GATE);
}

void idt_load()
{
    IdtDesc idtDesc;
    idtDesc.size = (sizeof(Idt)) - 1;
    idtDesc.offset = (uint64_t)&idt;
    idt_load_descriptor(&idtDesc);
}