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

void syscall_exit(InterruptFrame* interruptFrame)
{
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
    while (currentBlock != 0)
    {
        void* physicalAddress = process_allocate_pages(child, currentBlock->virtualAddress, currentBlock->pageAmount);

        memcpy(physicalAddress, currentBlock->physicalAddress, currentBlock->pageAmount * 0x1000);

        currentBlock = currentBlock->next;
    }

    InterruptFrame* childFrame = interrupt_frame_duplicate(interruptFrame);
    childFrame->cr3 = (uint64_t)child->pageDirectory;

    childFrame->rax = 0; // Child result
    interruptFrame->rax = child->id; // Parent result

    local_scheduler_push(child, childFrame, TASK_PRIORITY_MIN);

    local_scheduler_release();
}

void syscall_sleep(InterruptFrame* interruptFrame)
{
    local_scheduler_acquire();             

    Blocker blocker =
    {
        .timeout = time_nanoseconds() + SYSCALL_GET_ARG1(interruptFrame) * NANOSECONDS_PER_MILLISECOND
    };

    local_scheduler_block(interruptFrame, blocker);
    local_scheduler_schedule(interruptFrame);

    local_scheduler_release();
}

Syscall syscallTable[] =
{
    [SYS_EXIT] = (Syscall)syscall_exit,
    [SYS_FORK] = (Syscall)syscall_fork,
    [SYS_SLEEP] = (Syscall)syscall_sleep
};

void syscall_handler(InterruptFrame* interruptFrame)
{   
    uint64_t selector = interruptFrame->rax;

    //Temporary for testing
    if (selector == SYS_TEST)
    {
        tty_acquire();

        Cpu* cpu = smp_current_cpu();

        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        tty_print("CPU: "); 
        tty_printx(cpu->id); 
        tty_print(" | PID: "); 
        tty_printx((uint64_t)local_scheduler_running_task()->process->id);
        tty_print(" - "); 
        tty_print(string);
        tty_print("\n\r");

        tty_release();
        return;
    }

    if (selector < sizeof(syscallTable) / sizeof(Syscall))
    {
        syscallTable[selector](interruptFrame);
    }
    else
    {
        SYSCALL_SET_RESULT(interruptFrame, -1);
    }
}
