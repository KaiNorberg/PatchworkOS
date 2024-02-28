#include "syscall.h"

#include <stdint.h>

#include <lib-syscall.h>

#include <lib-asym.h>

#include "tty/tty.h"
#include "vmm/vmm.h"
#include "time/time.h"
#include "vfs/vfs.h"
#include "utils/utils.h"
#include "worker_pool/worker_pool.h"
#include "program_loader/program_loader.h"

#include "worker/file_table/file_table.h"
#include "worker/process/process.h"
#include "worker/scheduler/scheduler.h"
#include "worker/worker.h"

static inline uint8_t syscall_verify_pointer(const void* pointer, uint64_t size)
{
    if ((uint64_t)pointer > VMM_LOWER_HALF_MAX || (uint64_t)pointer + size > VMM_LOWER_HALF_MAX)
    {
        return 0;
    }

    return 1;
}

static inline uint8_t syscall_verify_string(const char* string)
{
    return syscall_verify_pointer(string, 0);
}

static inline void syscall_return_success(InterruptFrame* interruptFrame, uint64_t result)
{    
    interruptFrame->rax = result;
}

static inline void syscall_return_error(InterruptFrame* interruptFrame, Status status)
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
    const char* path = (const char*)SYSCALL_GET_ARG1(interruptFrame);

    if (!syscall_verify_string(path))
    {
        syscall_return_error(interruptFrame, STATUS_INVALID_POINTER);
        return;
    }

    int64_t pid = worker_pool_spawn(path);
    if (pid == -1)
    {
        syscall_return_error(interruptFrame, STATUS_FAILURE);
        return;
    }

    syscall_return_success(interruptFrame, pid);
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
    Status status = worker->scheduler->runningProcess->status;
    worker->scheduler->runningProcess->status = STATUS_SUCCESS;
    scheduler_release(worker->scheduler);

    syscall_return_success(interruptFrame, status);
}

void sys_map(InterruptFrame* interruptFrame)
{
    uint64_t lower = round_down(SYSCALL_GET_ARG1(interruptFrame), 0x1000);
    uint64_t upper = round_down(SYSCALL_GET_ARG2(interruptFrame), 0x1000);

    Worker* worker = worker_self();
    Process* process = worker->scheduler->runningProcess;

    process_allocate_pages(process, (void*)lower, SIZE_IN_PAGES(upper - lower));

    syscall_return_success(interruptFrame, 0);
}

void sys_open(InterruptFrame* interruptFrame)
{   
    const char* path = (const char*)SYSCALL_GET_ARG1(interruptFrame); 
    uint64_t flags = SYSCALL_GET_ARG2(interruptFrame);

    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    if (!syscall_verify_string(path))
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

    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    }

    if (!syscall_verify_pointer(buffer, length))
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

    FileTable* fileTable = worker_self()->scheduler->runningProcess->fileTable;

    File* file = file_table_get(fileTable, fd);
    if (file == 0)
    {
        syscall_return_error(interruptFrame, STATUS_DOES_NOT_EXIST);
        return;
    }

    if (!syscall_verify_pointer(buffer, length))
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
    [SYS_MAP] = (Syscall)sys_map,
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

        tty_set_column(0);
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
        const char* string = (const char*)SYSCALL_GET_ARG1(interruptFrame);
        if (string != 0)
        {
            tty_print(" | ");
            tty_print(string);
        }
        tty_print(" ");
        tty_printx(time_nanoseconds());
        tty_print("                                 ");

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