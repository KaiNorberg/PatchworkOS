#include "syscall.h"

#include "tty/tty.h"
#include "file_system/file_system.h"
#include "scheduler/scheduler.h"
#include "context/context.h"
#include "string/string.h"
#include "debug/debug.h"

#include "kernel/kernel.h"

#include "../common.h"

void syscall_init()
{

}

void syscall_handler(InterruptStackFrame* frame)
{    
    uint64_t out = 0;

    switch(frame->rax)
    {    
    case SYS_READ:  
    {

    }
    break;
    case SYS_WRITE:
    {

    }
    break;
    case SYS_FORK:
    {        
        Process* child = process_new((void*)frame->instructionPointer);
        context_save(child->context, frame);
        child->context->state.rax = 0;
        child->context->state.cr3 = (uint64_t)child->pageDirectory;

        Process* parent = scheduler_get_running_process();

        MemoryBlock* currentBlock = parent->firstMemoryBlock;
        while (1)
        {
            if (currentBlock == 0)
            {
                break;
            }

            void* physicalAddress = process_allocate_pages(child, currentBlock->virtualAddress, currentBlock->pageAmount);

            memcpy(physicalAddress, currentBlock->physicalAddress, currentBlock->pageAmount * 0x1000);

            currentBlock = currentBlock->next;
        }

        scheduler_append(child);

        out = 1234; //TODO: Replace with child pid, when pid is implemented
    }
    break;
    case SYS_EXIT:
    {
        Process* process = scheduler_get_running_process();

        scheduler_remove(process);
        process_free(process);

        context_load(scheduler_get_running_process()->context, frame);
    }
    break;
    case SYS_TEST:
    {
        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(frame), (void*)SYSCALL_GET_ARG1(frame));

        tty_print(string);
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    frame->rax = out;
}
