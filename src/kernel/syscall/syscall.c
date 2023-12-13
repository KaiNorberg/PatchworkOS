#include "syscall.h"

#include "tty/tty.h"
#include "file_system/file_system.h"
#include "multitasking/multitasking.h"
#include "string/string.h"

#include "kernel/kernel.h"

#include "../common.h"

void syscall_init()
{

}

void syscall_handler(InterruptStackFrame* frame)
{    
    uint64_t out;
    
    switch(frame->Registers.RAX)
    {
    case SYS_TEST:
    {
        uint64_t taskAddressSpace;
        asm volatile("movq %%cr3, %0" : "=r" (taskAddressSpace));

        VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);

        tty_print("Syscall test, rdi = "); tty_printi(frame->Registers.RDI); tty_print("!\n\r");

        VIRTUAL_MEMORY_LOAD_SPACE(taskAddressSpace);

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        uint64_t oldAddressSpace;
        asm volatile("movq %%cr3, %0" : "=r" (oldAddressSpace));

        Task* oldTask = get_running_task();

        memcpy(&(oldTask->Registers), &(frame->Registers), sizeof(frame->Registers));
        oldTask->InstructionPointer = frame->InstructionPointer;
        oldTask->StackPointer = frame->StackPointer;
        oldTask->AddressSpace = (VirtualAddressSpace*)oldAddressSpace;

        Task* newTask = load_next_task();

        memcpy(&(frame->Registers), &(newTask->Registers), sizeof(frame->Registers));
        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        VIRTUAL_MEMORY_LOAD_SPACE(newTask->AddressSpace);

        out = 0;
    }
    break;
    case SYS_EXIT:
    {
        Task* oldTask = get_running_task();

        Task* newTask = load_next_task();

        multitasking_free(oldTask);   

        memcpy(&(frame->Registers), &(newTask->Registers), sizeof(frame->Registers));

        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        VIRTUAL_MEMORY_LOAD_SPACE(newTask->AddressSpace);

        out = 0;
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    frame->Registers.RAX = out;
}
