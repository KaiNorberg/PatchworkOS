#pragma once

#include <stdint.h>

#define IDT_INTERRUPT 0b10001110
#define IDT_SYSCALL 0b11101110

#define IDT_VECTOR_AMOUNT 256

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

void idt_init();

void idt_load();

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

void remap_pic();