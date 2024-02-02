#include "syscall.h"

#include "tty/tty.h"
#include "string/string.h"

#include "worker_pool/worker_pool.h"

#include <lib-syscall.h>

void syscall_exit(InterruptFrame* interruptFrame)
{    
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);             

    scheduler_exit(worker->scheduler);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);             
}

void syscall_fork(InterruptFrame* interruptFrame)
{
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);             

    Process* child = process_new();

    Process* parent = worker->scheduler->runningTask->process;
    ProcessBlock* currentBlock = parent->firstBlock;
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

    scheduler_push(worker->scheduler, task_new(child, childFrame, TASK_PRIORITY_MIN));

    scheduler_release(worker->scheduler);             
}

void syscall_sleep(InterruptFrame* interruptFrame)
{
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);             

    Blocker blocker =
    {
        .timeout = time_nanoseconds() + SYSCALL_GET_ARG1(interruptFrame) * NANOSECONDS_PER_MILLISECOND
    };

    scheduler_block(worker->scheduler, interruptFrame, blocker);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);             
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

        Worker const* worker = worker_self();

        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        Point cursorPos = tty_get_cursor_pos();
        tty_set_cursor_pos(0, 16 * (worker->id + 2));

        tty_print("WORKER: "); 
        tty_printx(worker->id); 
        tty_print(" TASK AMOUNT: "); 
        tty_printx(scheduler_task_amount(worker->scheduler));
        if (worker->scheduler->runningTask != 0)
        {
            tty_print(" PID: "); 
            tty_printx(worker->scheduler->runningTask->process->id);
        }
        tty_print(" | ");
        tty_print(string);

        tty_set_cursor_pos(cursorPos.x, cursorPos.y);

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
