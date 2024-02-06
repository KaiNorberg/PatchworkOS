#include "syscall.h"

#include "tty/tty.h"
#include "worker_pool/worker_pool.h"

#include "worker/program_loader/program_loader.h"

#include <lib-syscall.h>
#include <libc/string.h>

inline void* syscall_verify_pointer(PageDirectory const* pageDirectory, void* pointer, uint64_t size)
{
    if ((uint64_t)pointer >= (uint64_t)USER_ADDRESS_SPACE_TOP)
    {
        return 0;
    }

    void* start = page_directory_get_physical_address(pageDirectory, pointer);
    if (start == 0 || (uint64_t)start >= USER_ADDRESS_SPACE_TOP)
    {
        return 0;
    }
    void const* end = page_directory_get_physical_address(pageDirectory, (void*)((uint64_t)pointer + size));
    if (end == 0 || (uint64_t)end >= (uint64_t)USER_ADDRESS_SPACE_TOP)
    {
        return 0;
    }

    if (end != start + size)
    {
        return 0;
    }

    return start;
}

inline char* syscall_verify_string(PageDirectory const* pageDirectory, char* string)
{
    if (syscall_verify_pointer(pageDirectory, string, 0))
    {
        return (char*)syscall_verify_pointer(pageDirectory, string, strlen(string));
    }
    else
    {
        return 0;
    }
}

inline void syscall_return_success(InterruptFrame* interruptFrame, uint64_t result)
{    
    interruptFrame->rax = result;
}

inline void syscall_return_error(InterruptFrame* interruptFrame, Status status)
{    
    Worker* worker = worker_self();
    if (worker->scheduler->runningProcess != 0)
    {
        worker->scheduler->runningProcess->status = status;    
        interruptFrame->rax = -1;
    }
}

void sys_exit(InterruptFrame* interruptFrame)
{    
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);

    scheduler_exit(worker->scheduler);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);
}

void sys_spawn(InterruptFrame* interruptFrame)
{
    PageDirectory const* pageDirectory = SYSCALL_GET_PAGE_DIRECTORY(interruptFrame);

    Worker* worker = worker_self();
    scheduler_acquire(worker->scheduler);

    const char* path = syscall_verify_string(pageDirectory, (char*)SYSCALL_GET_ARG1(interruptFrame));
    if (path != 0)
    {                    
        Process* process = process_new(PROCESS_PRIORITY_MIN);
        Status status = load_program(process, path);
        if (status == STATUS_SUCCESS)
        {            
            scheduler_push(worker->scheduler, process);
            syscall_return_success(interruptFrame, process->id);
        }
        else
        {
            process_free(process);
            syscall_return_error(interruptFrame, status);
        }
    }
    else
    {
        syscall_return_error(interruptFrame, STATUS_INVALID_POINTER);
    }
    
    scheduler_release(worker->scheduler);
}

void sys_sleep(InterruptFrame* interruptFrame)
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
    syscall_return_success(interruptFrame, 0);
}

void sys_status(InterruptFrame* interruptFrame)
{
    Worker* worker = worker_self();
    scheduler_acquire(worker->scheduler);

    Status status;
    if (worker->scheduler->runningProcess != 0)
    {
        status = worker->scheduler->runningProcess->status;
        worker->scheduler->runningProcess->status = STATUS_SUCCESS;
    }
    else
    {
        status = STATUS_FAILURE;
    }

    scheduler_release(worker->scheduler);
    syscall_return_success(interruptFrame, status);
}

Syscall syscallTable[] =
{
    [SYS_EXIT] = (Syscall)sys_exit,
    [SYS_SPAWN] = (Syscall)sys_spawn,
    [SYS_SLEEP] = (Syscall)sys_sleep,
    [SYS_STATUS] = (Syscall)sys_status
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
        tty_print(" PROCESS AMOUNT: "); 
        tty_printx(scheduler_process_amount(worker->scheduler));
        if (worker->scheduler->runningProcess != 0)
        {
            tty_print(" PID: "); 
            tty_printx(worker->scheduler->runningProcess->id);
        }
        tty_print(" | ");
        tty_print(string);
        tty_print(" ");
        tty_printx(time_nanoseconds());

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
        syscall_return_error(interruptFrame, STATUS_NOT_ALLOWED);
    }
}
