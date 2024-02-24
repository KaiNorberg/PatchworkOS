#include "syscall.h"

#include <lib-syscall.h>
#include <stdint.h>

#include "tty/tty.h"
#include "worker_pool/worker_pool.h"
#include "worker/program_loader/program_loader.h"
#include "lib-asym.h"
#include "time/time.h"
#include "vfs/vfs.h"
#include "worker/file_table/file_table.h"
#include "worker/process/process.h"
#include "worker/scheduler/scheduler.h"
#include "worker/worker.h"

inline void* syscall_verify_pointer(PageDirectory const* pageDirectory, void* pointer, uint64_t size)
{
    return 0;
}

inline char* syscall_verify_string(PageDirectory const* pageDirectory, char* string)
{
    return 0;
}

inline void syscall_return_success(InterruptFrame* interruptFrame, uint64_t result)
{    
    interruptFrame->rax = result;
}

inline void syscall_return_error(InterruptFrame* interruptFrame, Status status)
{    
    Worker* worker = worker_self();
    worker->scheduler->runningProcess->status = status;    
    interruptFrame->rax = -1;
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
    uint64_t fd = SYSCALL_GET_ARG1(interruptFrame);

    Worker* worker = worker_self();
    FileTable* fileTable = worker->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    }

    Process* process = process_new(PROCESS_PRIORITY_MIN);
    Status status = load_program(process, file);
    if (status != STATUS_SUCCESS)
    {
        process_free(process);
        syscall_return_error(interruptFrame, status);
        return;
    }

    scheduler_acquire(worker->scheduler);
    scheduler_push(worker->scheduler, process);
    scheduler_release(worker->scheduler);

    syscall_return_success(interruptFrame, process->id);
}

void sys_sleep(InterruptFrame* interruptFrame)
{
    uint64_t milliseconds = SYSCALL_GET_ARG1(interruptFrame);

    Worker* worker = worker_self();
    scheduler_acquire(worker->scheduler);

    uint64_t timeout = time_nanoseconds() + milliseconds * NANOSECONDS_PER_MILLISECOND;
    scheduler_block(worker->scheduler, interruptFrame, timeout);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);
    syscall_return_success(interruptFrame, 0);
}

void sys_status(InterruptFrame* interruptFrame)
{
    Worker* worker = worker_self();
    scheduler_acquire(worker->scheduler);

    Status status;
    status = worker->scheduler->runningProcess->status;
    worker->scheduler->runningProcess->status = STATUS_SUCCESS;

    scheduler_release(worker->scheduler);
    syscall_return_success(interruptFrame, status);
}

void sys_open(InterruptFrame* interruptFrame)
{   
    char* path = (char*)SYSCALL_GET_ARG1(interruptFrame); 
    uint64_t flags = SYSCALL_GET_ARG2(interruptFrame);

    PageDirectory const* pageDirectory = SYSCALL_GET_PAGE_DIRECTORY(interruptFrame);
    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    path = syscall_verify_string(pageDirectory, path);
    if (path == 0)
    {              
        syscall_return_error(interruptFrame, STATUS_INVALID_POINTER);
        return;
    } 

    uint64_t fd;
    Status status = file_table_open(fileTable, &fd, path, flags);
    if (status != STATUS_SUCCESS)
    {            
        syscall_return_error(interruptFrame, status);
        return;
    }
    
    syscall_return_success(interruptFrame, fd);
}

void sys_close(InterruptFrame* interruptFrame)
{    
    uint64_t fd = SYSCALL_GET_ARG1(interruptFrame);

    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;
    
    Status status = file_table_close(fileTable, fd);
    if (status != STATUS_SUCCESS)
    {
        syscall_return_error(interruptFrame, status);
        return;
    }
    
    syscall_return_success(interruptFrame, 0);
}   

void sys_read(InterruptFrame* interruptFrame)
{
    uint64_t fd = SYSCALL_GET_ARG1(interruptFrame);
    void* buffer = (void*)SYSCALL_GET_ARG2(interruptFrame);
    uint64_t length = SYSCALL_GET_ARG3(interruptFrame); 

    PageDirectory const* pageDirectory = SYSCALL_GET_PAGE_DIRECTORY(interruptFrame);
    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    }

    buffer = syscall_verify_pointer(pageDirectory, buffer, length);
    if (buffer == 0)
    {              
        syscall_return_error(interruptFrame, STATUS_INVALID_POINTER);
        return;
    } 

    Status status = vfs_read(file, buffer, length);
    if (status != STATUS_SUCCESS)
    {
        syscall_return_error(interruptFrame, status);
        return;
    }
    
    syscall_return_success(interruptFrame, 0);
}

void sys_write(InterruptFrame* interruptFrame)
{
    uint64_t fd = SYSCALL_GET_ARG1(interruptFrame);
    void* buffer = (void*)SYSCALL_GET_ARG2(interruptFrame);
    uint64_t length = SYSCALL_GET_ARG3(interruptFrame); 

    PageDirectory const* pageDirectory = SYSCALL_GET_PAGE_DIRECTORY(interruptFrame);
    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    }

    buffer = syscall_verify_pointer(pageDirectory, buffer, length);
    if (buffer == 0)
    {              
        syscall_return_error(interruptFrame, STATUS_INVALID_POINTER);
        return;
    } 

    Status status = vfs_write(file, buffer, length);
    if (status != STATUS_SUCCESS)
    {
        syscall_return_error(interruptFrame, status);
        return;
    }
    
    syscall_return_success(interruptFrame, 0);
}

void sys_seek(InterruptFrame* interruptFrame)
{
    uint64_t fd = SYSCALL_GET_ARG1(interruptFrame);
    int64_t offset = SYSCALL_GET_ARG2(interruptFrame);
    uint64_t origin = SYSCALL_GET_ARG3(interruptFrame); 

    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    } 

    Status status = vfs_seek(file, offset, origin);
    if (status != STATUS_SUCCESS)
    {
        syscall_return_error(interruptFrame, status);
        return;
    }

    syscall_return_success(interruptFrame, 0);
}

Syscall syscallTable[] =
{
    [SYS_EXIT] = (Syscall)sys_exit,
    [SYS_SPAWN] = (Syscall)sys_spawn,
    [SYS_SLEEP] = (Syscall)sys_sleep,
    [SYS_STATUS] = (Syscall)sys_status,
    [SYS_OPEN] = (Syscall)sys_open,
    [SYS_CLOSE] = (Syscall)sys_close,
    [SYS_READ] = (Syscall)sys_read,
    [SYS_WRITE] = (Syscall)sys_write,
    [SYS_SEEK] = (Syscall)sys_seek
};

void syscall_handler(InterruptFrame* interruptFrame)
{   
    uint64_t selector = interruptFrame->rax;

    //Temporary for testing
    if (selector == SYS_TEST)
    {
        tty_acquire();

        Worker const* worker = worker_self();

        //const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        tty_set_row(worker->id + 2);

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
        //tty_print(string);
        tty_print(" ");
        tty_printx(time_nanoseconds());

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