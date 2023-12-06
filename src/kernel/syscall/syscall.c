#include "syscall.h"

#include "kernel/tty/tty.h"
#include "kernel/file_system/file_system.h"
#include "kernel/multitasking/multitasking.h"
#include "kernel/string/string.h"

#include "common.h"

VirtualAddressSpace* syscallAddressSpace;

void syscall_init(VirtualAddressSpace* addressSpace)
{
    syscallAddressSpace = addressSpace;
}

uint64_t syscall_handler(RegisterBuffer* registerBuffer, InterruptStackFrame* frame)
{   
    //TODO: Finish return system 

    uint64_t taskAddressSpace;
    asm volatile("movq %%cr3, %0" : "=r" (taskAddressSpace));

    VIRTUAL_MEMORY_LOAD_SPACE(syscallAddressSpace);
    
    uint64_t out;

    switch(registerBuffer->RAX)
    {
    case SYS_TEST:
    {
        tty_print("Syscall test!\n\r");

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        Task* oldTask = get_running_task();

        memcpy(&(oldTask->Registers), registerBuffer, sizeof(RegisterBuffer));
        oldTask->InstructionPointer = frame->InstructionPointer;
        oldTask->StackPointer = frame->StackPointer;
        oldTask->AddressSpace = (VirtualAddressSpace*)taskAddressSpace;

        Task* newTask = load_next_task(); 
        
        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));
        
        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        taskAddressSpace = (uint64_t)newTask->AddressSpace;

        out = 0;
    }
    break;
    case SYS_EXIT:
    {    
        Task* oldTask = get_running_task();

        Task* newTask = load_next_task(); 

        erase_task(oldTask);   

        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));

        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        taskAddressSpace = (uint64_t)newTask->AddressSpace;

        out = 0;
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    VIRTUAL_MEMORY_LOAD_SPACE(taskAddressSpace);

    return 0;
}
