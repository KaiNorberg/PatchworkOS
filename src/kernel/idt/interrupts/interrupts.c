#include "interrupts.h"

#include "kernel/io/io.h"

#include "kernel/tty/tty.h"
#include "kernel/utils/utils.h"

#include "kernel/syscall/syscall.h"

__attribute__((interrupt)) void keyboard_interrupt(void* frame)
{        
    uint8_t scanCode = io_inb(0x60);

    tty_put(scanCode);

    /*KeyBoard::HandleScanCode(ScanCode);

    if (!(ScanCode & (0b10000000))) //If key was pressed down
    {
        ProcessHandler::KeyBoardInterupt();
    }*/

    io_outb(PIC1_COMMAND, PIC_EOI);
}
/*
__attribute__((interrupt)) void syscall_interrupt(void* frame)
{       
    uint64_t rax;
    asm volatile("movq %%rax, %0" : "=r" (rax));
    uint64_t rdi;
    asm volatile("movq %%rdi, %0" : "=r" (rdi));
    uint64_t rsi;
    asm volatile("movq %%rsi, %0" : "=r" (rsi));
    uint64_t rdx;
    asm volatile("movq %%rdx, %0" : "=r" (rdx));

    rax = syscall(rax, rdi, rsi, rdx);
    asm volatile("movq %0, %%rax" : : "r"(rax));
    
    io_outb(PIC1_COMMAND, PIC_EOI);
}*/