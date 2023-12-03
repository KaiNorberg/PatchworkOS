#include "interrupts.h"

#include "kernel/io/io.h"

#include "kernel/tty/tty.h"
#include "kernel/utils/utils.h"

#include "kernel/syscall/syscall.h"
#include "kernel/multitasking/multitasking.h"

#include "kernel/string/string.h"

#define ENTER 0x1C
#define BACKSPACE 0x0E
#define CONTROL 0x1D
#define LEFT_SHIFT 0x2A
#define ARROW_UP 0x48
#define ARROW_DOWN 0x50
#define ARROW_LEFT 0x4B
#define ARROW_RIGHT 0x4D
#define PAGE_UP 0x49
#define PAGE_DOWN 0x51
#define CAPS_LOCK 0x3A

const char SCAN_CODE_TABLE[] =
{
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
    '9', '0', '-', '=', BACKSPACE,	/* Backspace */
    '\t',			/* Tab */
    'q', 'w', 'e', 'r',	/* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', ENTER,	/* Enter key */
    CONTROL,			/* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
    '\'', '`', LEFT_SHIFT,		/* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
    'm', ',', '.', '/',   0,				/* Right shift */
    '*',
    0,	/* Alt */
    ' ',	/* Space bar */
    CAPS_LOCK,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    ARROW_UP,	/* Up Arrow */
    PAGE_UP,	/* Page Up */
    '-',
    ARROW_LEFT,	/* Left Arrow */
    0,
    ARROW_RIGHT,	/* Right Arrow */
    '+',
    0,	/* 79 - End key*/
    ARROW_DOWN,	/* Down Arrow */
    PAGE_DOWN,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

const char SHIFTED_SCAN_CODE_TABLE[] =
{
    0,  0, '!', '"', '#', '$', '%', '&', '/', '(',	/* 9 */
    ')', '=', '-', '=', BACKSPACE,	/* Backspace */
    '\t',			/* Tab */
    'Q', 'W', 'E', 'R',	/* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', ENTER,	/* Enter key */
    CONTROL,			/* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',	/* 39 */
    '\'', '`', LEFT_SHIFT,		/* Left shift */
    '\\', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
    'M', ',', '.', '/',   0,				/* Right shift */
    '*',
    0,	/* Alt */
    ' ',	/* Space bar */
    CAPS_LOCK,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    ARROW_UP,	/* Up Arrow */
    PAGE_UP,	/* Page Up */
    '-',
    ARROW_LEFT,	/* Left Arrow */
    0,
    ARROW_RIGHT,	/* Right Arrow */
    '+',
    0,	/* 79 - End key*/
    ARROW_DOWN,	/* Down Arrow */
    PAGE_DOWN,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

__attribute__((interrupt)) void keyboard_interrupt(InterruptStackFrame* frame)
{        
    uint8_t scanCode = io_inb(0x60);

    if (!(scanCode & (0b10000000))) //If key was pressed down
    {
        tty_put(SCAN_CODE_TABLE[scanCode]);
    }

    io_outb(PIC1_COMMAND, PIC_EOI);
}

__attribute__((interrupt)) void syscall_interrupt(InterruptStackFrame* frame)
{   
    //This is only sys_yield for now
    //TODO: implement address space changes
    RegisterBuffer* registerBuffer;

    asm volatile("mov %%rsp, %0" : "=r" (registerBuffer));
    
    registerBuffer = (RegisterBuffer*)((uint64_t)registerBuffer + 8);

    Task* oldTask = get_running_task();

    memcpy(&(oldTask->Registers), registerBuffer, sizeof(RegisterBuffer));
    oldTask->InstructionPointer = frame->InstructionPointer;
    oldTask->StackPointer = frame->StackPointer;

    Task* newTask = load_next_task(); 
    
    registerBuffer->R12 = newTask->Registers.R12; 
    registerBuffer->R11 = newTask->Registers.R11; 
    registerBuffer->R10 = newTask->Registers.R10; 
    registerBuffer->R9 = newTask->Registers.R9; 
    registerBuffer->R8 = newTask->Registers.R8; 
    registerBuffer->RBP = newTask->Registers.RBP;
    registerBuffer->RDI = newTask->Registers.RDI;
    registerBuffer->RSI = newTask->Registers.RSI;
    registerBuffer->RBX = newTask->Registers.RBX;
    registerBuffer->RCX = newTask->Registers.RCX;
    registerBuffer->RDX = newTask->Registers.RDX;
    registerBuffer->RAX = newTask->Registers.RAX;

    frame->InstructionPointer = newTask->InstructionPointer;
    frame->StackPointer = newTask->StackPointer;
}