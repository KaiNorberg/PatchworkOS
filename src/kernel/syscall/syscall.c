#include "syscall.h"

#include <stdint.h>

#include <lib-syscall.h>

#include <lib-asym.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "vmm/vmm.h"
#include "time/time.h"
#include "vfs/vfs.h"
#include "utils/utils.h"
#include "smp/smp.h"
#include "debug/debug.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"
#include "program_loader/program_loader.h"

static inline uint8_t verify_pointer(const void* pointer, uint64_t size)
{
    if ((uint64_t)pointer > VMM_LOWER_HALF_MAX || (uint64_t)pointer + size > VMM_LOWER_HALF_MAX)
    {
        return 0;
    }

    return 1;
}

static inline uint8_t verify_string(const char* string)
{
    return verify_pointer(string, 0);
}

/*void sys_exit(InterruptFrame* interruptFrame)
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
}*/

int64_t syscall_exit(Status status)
{
    scheduler_exit(status);

    debug_panic("Returned from exit");
    return 0;
}

int64_t syscall_spawn(const char* path)
{
    if (!verify_string(path))
    {
        scheduler_thread()->status = STATUS_INVALID_POINTER;
        return -1;
    }

    int64_t pid = scheduler_spawn(path);
    if (pid == -1)
    {
        scheduler_thread()->status = STATUS_FAILURE;
        return -1;
    }

    return pid;
}

Status syscall_status()
{
    Thread* thread = scheduler_thread();
    Status temp = thread->status;
    thread->status = STATUS_FAILURE;
    return temp;
}

int64_t syscall_test(const char* string)
{
    interrupts_disable();

    Cpu* self = smp_self();
    tty_acquire();
    
    uint8_t oldRow = tty_get_row();
    uint8_t oldColumn = tty_get_column();

    tty_set_column(0);
    tty_set_row(self->id);

    tty_print("CPU: ");
    tty_printx(self->id); 
    tty_print(" THREAD AMOUNT: "); 
    tty_printx(scheduler_local_thread_amount());
    tty_print(" PID: "); 
    tty_printx(scheduler_process()->id);
    tty_print(" TID: "); 
    tty_printx(scheduler_thread()->id);
    if (string != 0)
    {
        tty_print(" | ");
        tty_print(string);
    }
    tty_print(" ");
    tty_printx(time_nanoseconds());
    tty_print("                                 ");

    tty_set_row(oldRow);
    tty_set_column(oldColumn);

    tty_release();

    interrupts_enable();

    scheduler_thread()->status = STATUS_SUCCESS;
    return 0;
}

void* syscallTable[] =
{
    [SYS_EXIT] = (void*)syscall_exit,
    [SYS_SPAWN] =  (void*)syscall_spawn,
    [SYS_SLEEP] = (void*)syscall_test,
    [SYS_STATUS] = (void*)syscall_status,
    [SYS_MAP] = (void*)syscall_test,
    [SYS_OPEN] = (void*)syscall_test,
    [SYS_CLOSE] = (void*)syscall_test,
    [SYS_READ] = (void*)syscall_test,
    [SYS_WRITE] = (void*)syscall_test,
    [SYS_SEEK] = (void*)syscall_test,
    [SYS_TEST] = (void*)syscall_test
};