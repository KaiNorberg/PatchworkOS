#pragma once

#include <stdint.h>

#define IDT_INTERRUPT_GATE 0b1110
#define IDT_TRAP_GATE 0b1111

#define IDT_RING0 0b00
#define IDT_RING1 0b01
#define IDT_RING2 0b10
#define IDT_RING3 0b11

#define IDT_VECTOR_AMOUNT 256

#define IDT_EXCEPTION_AMOUNT 0x20

typedef struct __attribute__((packed))
{
	uint16_t isrLow;      
	uint16_t codeSegment;
	uint8_t	ist;
	uint8_t attributes;
	uint16_t isrMid;
	uint32_t isrHigh;
	uint32_t reserved;
} IdtEntry;

typedef struct __attribute__((packed))
{
	uint16_t size;
	uint64_t offset;
} IdtDesc;

typedef struct __attribute__((packed))
{
	IdtEntry entries[IDT_VECTOR_AMOUNT];
} Idt;

extern void idt_load_descriptor(IdtDesc* descriptor);

void idt_load(Idt* idt);

void idt_set_vector(Idt* idt, uint8_t vector, void* isr, uint8_t privilageLevel, uint8_t gateType);