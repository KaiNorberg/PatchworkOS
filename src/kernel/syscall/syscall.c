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

void syscall_handler(InterruptStackFrame* frame, VirtualAddressSpace** addressSpace)
{    
    uint64_t out;
    
    switch(frame->Registers.RAX)
    {
    case SYS_TEST:
    {
        tty_print("Syscall test, rdi = "); tty_printi(frame->Registers.RDI); tty_print("!\n\r");

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        Task* oldTask = get_running_task();

        memcpy(&(oldTask->Registers), &(frame->Registers), sizeof(frame->Registers));
        oldTask->InstructionPointer = frame->InstructionPointer;
        oldTask->StackPointer = frame->StackPointer;
        oldTask->AddressSpace = *addressSpace;

        Task* newTask = load_next_task();

        memcpy(&(frame->Registers), &(newTask->Registers), sizeof(frame->Registers));
        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        *addressSpace = newTask->AddressSpace;

        out = 0;
    }
    break;
    case SYS_EXIT:
    {
        Task* oldTask = get_running_task();

        Task* newTask = load_next_task();

        //Temporary code for testing purposes
        if (newTask == 0)
        {
            while (1)
            {
                asm volatile ("HLT");
            }
        }

        multitasking_free(oldTask);   

        memcpy(&(frame->Registers), &(newTask->Registers), sizeof(frame->Registers));

        frame->InstructionPointer = newTask->InstructionPointer;
        frame->StackPointer = newTask->StackPointer;

        *addressSpace = newTask->AddressSpace;

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
