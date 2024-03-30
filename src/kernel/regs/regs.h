#pragma once

#include "defs/defs.h"

#define MSR_LOCAL_APIC 0x1B
#define MSR_CPU_ID 0xC0000103 //IA32_TSC_AUX

#define RFLAGS_ALWAYS_SET (1 << 1) 
#define RFLAGS_INTERRUPT_ENABLE (1 << 9) 

#define CR4_PAGE_GLOBAL_ENABLE (1 << 7)

#define MSR_READ(msr) ({ \
    uint32_t low, high; \
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr)); \
    ((uint64_t)high << 32) | ((uint64_t)low); \
})

#define MSR_WRITE(msr, value) ({ \
    uint32_t low = (uint32_t)(((uint64_t)value) & 0xFFFFFFFF); \
    uint32_t high = (uint32_t)(((uint64_t)value) >> 32); \
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high)); \
})

#define RFLAGS_READ() ({ \
    uint64_t rflags; \
    asm volatile("pushfq; pop %0" : "=r"(rflags)); \
    rflags; \
})

#define RFLAGS_WRITE(value) ({ \
    asm volatile("push %0; popfq" : : "r"(value)); \
})

#define CR4_READ() ({ \
    uint64_t cr4; \
    asm volatile("mov %%cr4, %0" : "=r"(cr4)); \
    cr4; \
})

#define CR4_WRITE(value) ({ \
    asm volatile("mov %0, %%cr4" : : "r"(value)); \
})

#define CR3_READ() ({ \
    uint64_t cr3; \
    asm volatile("mov %%cr3, %0" : "=r"(cr3)); \
    cr3; \
})

#define CR3_WRITE(value) ({ \
    asm volatile("mov %0, %%cr3" : : "r"(value)); \
})

#define CR2_READ() ({ \
    uint64_t cr2; \
    asm volatile("mov %%cr2, %0" : "=r"(cr2)); \
    cr2; \
})

#define CR2_WRITE(value) ({ \
    asm volatile("mov %0, %%cr2" : : "r"(value)); \
})
