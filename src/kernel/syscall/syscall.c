#include "syscall.h"

#include "tty/tty.h"
#include "ram_disk/ram_disk.h"
#include "scheduler/scheduler.h"
#include "interrupt_frame/interrupt_frame.h"
#include "string/string.h"
#include "debug/debug.h"
#include "smp/smp.h"
#include "time/time.h"
#include "hpet/hpet.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"

#include "kernel/kernel.h"

#include <lib-syscall.h>

void (*syscallTable[SYSCALL_TABLE_LENGTH])(InterruptFrame* interruptFrame);

void syscall_stub(InterruptFrame* interruptFrame)
{
    SYSCALL_SET_RESULT(interruptFrame, -1);
}

void syscall_exit(InterruptFrame* interruptFrame)
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

void syscall_fork(InterruptFrame* interruptFrame)
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
    childFrame->cr3 = (uint64_t)child->pageDirectory;

    childFrame->rax = 0; // Child result
    interruptFrame->rax = 1234; // Parent result

    local_scheduler_push(child, childFrame, TASK_PRIORITY_EXPRESS);

    local_scheduler_release();
}

void syscall_sleep(InterruptFrame* interruptFrame)
{
    /*local_scheduler_acquire();             

    Blocker* blocker = kmalloc(sizeof(Blocker));
    blocker->timeout = time_nanoseconds() + SYSCALL_GET_ARG1(interruptFrame);

    local_scheduler_block(blocker);
    local_scheduler_schedule(interruptFrame);

    local_scheduler_release();*/
}

void syscall_table_init()
{
    tty_start_message("Syscall Table initializing");

    for (uint64_t i = 0; i < SYSCALL_TABLE_LENGTH; i++)
    {
        syscallTable[i] = syscall_stub;
    }

    syscallTable[SYS_EXIT] = syscall_exit;
    syscallTable[SYS_FORK] = syscall_fork;
    syscallTable[SYS_SLEEP] = syscall_sleep;

    tty_end_message(TTY_MESSAGE_OK);
}

void syscall_handler(InterruptFrame* interruptFrame)
{   
    uint64_t selector = interruptFrame->rax;

    //Temporary for testing
    if (selector == SYS_TEST)
    {
        tty_acquire();

        Cpu* cpu = smp_current_cpu();

        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        Point cursorPos = tty_get_cursor_pos();

        tty_set_cursor_pos(0, 16 * cpu->id);
        tty_print("CPU "); 
        tty_printx(cpu->id); 
        tty_print(": "); 
        tty_printx((uint64_t)local_scheduler_running_task());
        tty_print(" - "); 
        tty_printx((uint64_t)local_scheduler_task_amount());
        tty_print(" | "); 
        tty_print(string);

        tty_set_cursor_pos(cursorPos.x, cursorPos.y);

        tty_release();
        return;
    }

    if (selector < SYSCALL_TABLE_LENGTH)
    {
        syscallTable[selector](interruptFrame);
    }
    else
    {
        SYSCALL_SET_RESULT(interruptFrame, -1);
    }
}
