#include "syscall.h"

#include "tty/tty.h"
#include "file_system/file_system.h"
#include "scheduler/scheduler.h"
#include "interrupt_frame/interrupt_frame.h"
#include "string/string.h"
#include "debug/debug.h"

#include "kernel/kernel.h"

#include "../common.h"

void syscall_handler(InterruptFrame* interruptFrame)
{    
    uint64_t out = 0;

    switch(interruptFrame->rax)
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
        Process* child = process_new((void*)interruptFrame->instructionPointer);
        interrupt_frame_copy(child->interruptFrame, interruptFrame);
        child->interruptFrame->rax = 0;
        child->interruptFrame->cr3 = (uint64_t)child->pageDirectory;

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

        scheduler_schedule();

        interrupt_frame_copy(interruptFrame, scheduler_get_running_process()->interruptFrame);
    }
    break;
    case SYS_TEST:
    {
        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        tty_print(string);
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    interruptFrame->rax = out;
}
