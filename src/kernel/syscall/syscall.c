#include "syscall.h"

#include "tty/tty.h"
#include "file_system/file_system.h"
#include "multitasking/multitasking.h"
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
        Task* child = multitasking_new((void*)frame->instructionPointer);
        context_save(child->context, frame);
        child->context->state.rax = 0;
        child->context->state.cr3 = (uint64_t)child->pageDirectory;

        Task* parent = multitasking_get_running_task();

        MemoryBlock* currentBlock = parent->firstMemoryBlock;
        while (1)
        {
            if (currentBlock == 0)
            {
                break;
            }

            void* physicalAddress = task_allocate_pages(child, currentBlock->virtualAddress, currentBlock->pageAmount);

            memcpy(physicalAddress, currentBlock->physicalAddress, currentBlock->pageAmount * 0x1000);

            currentBlock = currentBlock->next;
        }

        out = 1234; //TODO: Replace with child pid, when pid is implemented
    }
    break;
    case SYS_EXIT:
    {
        multitasking_free(multitasking_get_running_task());

        context_load(multitasking_get_running_task()->context, frame);
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
