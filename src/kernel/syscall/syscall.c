#include "syscall.h"

#include "tty/tty.h"
#include "ram_disk/ram_disk.h"
#include "scheduler/scheduler.h"
#include "interrupt_frame/interrupt_frame.h"
#include "string/string.h"
#include "debug/debug.h"
#include "smp/smp.h"

#include "kernel/kernel.h"

#include "../common.h"

void syscall_handler(InterruptFrame* interruptFrame)
{    
    uint64_t out = 0;

    switch(interruptFrame->rax)
    {    
    case SYS_FORK:
    {   
        local_scheduler_acquire();             

        Process* child = process_new();

        Process* parent = local_scheduler_running_task()->process;
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

        InterruptFrame* childFrame = interrupt_frame_duplicate(interruptFrame);
        childFrame->rax = 0; 
        childFrame->cr3 = (uint64_t)child->pageDirectory;

        local_scheduler_release();

        scheduler_push(child, childFrame);

        out = 1234; //TODO: Replace with child pid, when pid is implemented
    }
    break;
    case SYS_EXIT:
    {    
        //Temporary for testing
        tty_acquire();

        Cpu* cpu = smp_current_cpu();

        Point cursorPos = tty_get_cursor_pos();

        tty_set_cursor_pos(0, 16 * cpu->id);
        tty_print("CPU "); tty_printx(cpu->id); tty_print(": "); tty_printx(0); tty_print("                                                 ");

        tty_set_cursor_pos(cursorPos.x, cursorPos.y);

        tty_release();

        local_scheduler_acquire();

        local_scheduler_exit();
        local_scheduler_schedule(interruptFrame);

        local_scheduler_release();
    }
    break;
    case SYS_TEST: //Temporary for testing
    {
        tty_acquire();

        Cpu* cpu = smp_current_cpu();

        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        Point cursorPos = tty_get_cursor_pos();

        tty_set_cursor_pos(0, 16 * cpu->id);
        tty_print("CPU "); tty_printx(cpu->id); tty_print(": "); tty_printx((uint64_t)local_scheduler_running_task()); tty_print(" | "); tty_print(string);

        tty_set_cursor_pos(cursorPos.x, cursorPos.y);

        tty_release();
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
