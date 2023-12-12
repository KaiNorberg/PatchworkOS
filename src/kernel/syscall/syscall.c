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

void syscall_handler(RegisterBuffer* registerBuffer, InterruptStackFrame* frame)
{    
    uint64_t out;

    switch(registerBuffer->RAX)
    {
    case SYS_TEST:
    {
        //uint64_t taskAddressSpace;
        //asm volatile("movq %%cr3, %0" : "=r" (taskAddressSpace));

        //VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);

        tty_print("Syscall test, rdi = "); tty_printi(registerBuffer->RDI); tty_print("!\n\r");

        //VIRTUAL_MEMORY_LOAD_SPACE(taskAddressSpace);

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        uint64_t oldAddressSpace;
        asm volatile("movq %%cr3, %0" : "=r" (oldAddressSpace));

        Task* oldTask = get_running_task();

        memcpy(&(oldTask->Registers), registerBuffer, sizeof(RegisterBuffer));
        oldTask->InstructionPointer = frame->InstructionPointer;
        oldTask->StackPointer = frame->StackPointer;
        oldTask->AddressSpace = (VirtualAddressSpace*)oldAddressSpace;

        Task* newTask = load_next_task();

        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));
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

        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));

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

    registerBuffer->RAX = out;
}
