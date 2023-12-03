#include "syscall.h"

#include "kernel/tty/tty.h"
#include "kernel/file_system/file_system.h"
#include "kernel/multitasking/multitasking.h"
#include "kernel/string/string.h"

#include "common.h"

VirtualAddressSpace* syscallAddressSpace;
uint64_t* syscallStack;

uint64_t(*syscallTable[SYS_AMOUNT])(void);
uint64_t syscallAmount = SYS_AMOUNT;

uint64_t syscall_yield()
{   
    tty_print("Syscall yield test\n\r");

    Task* nextTask = load_next_task();

    return nextTask;
}

uint64_t syscall_exit()
{   
    tty_print("Syscall exit test\n\r");
    return 0;
}

void syscall_init(VirtualAddressSpace* addressSpace, uint64_t* stack)
{
    syscallAddressSpace = addressSpace;
    syscallStack = stack;

    syscallTable[SYS_YIELD] = syscall_yield;
    syscallTable[SYS_EXIT] = syscall_exit;
}

uint64_t syscall_handler(RegisterBuffer* registerBuffer, InterruptStackFrame* frame)
{
    //TODO: implement return system

    switch(registerBuffer->RAX)
    {
    case SYS_YIELD:
    {
        Task* oldTask = get_running_task();

        memcpy(&(oldTask->Registers), registerBuffer, sizeof(RegisterBuffer));
        oldTask->InstructionPointer = frame->InstructionPointer;
        oldTask->StackPointer = frame->StackPointer;
        //oldTask->AddressSpace = frame->StackPointer; //Add this

        Task* newTask = load_next_task(); 
        
        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));
        
        virtual_memory_load_space(newTask->AddressSpace);

        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        return 0;
    }
    break;
    case SYS_EXIT:
    {
        Task* oldTask = get_running_task();

        Task* newTask = load_next_task(); 
        
        //Detach old task from linked list
        oldTask->Next->Prev = oldTask->Prev;
        oldTask->Prev->Next = oldTask->Next;

        //TODO:Free old task memory

        memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));
        
        virtual_memory_load_space(newTask->AddressSpace);

        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        return 0;
    }
    break;
    }
}
