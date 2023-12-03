#include "syscall.h"

#include "kernel/tty/tty.h"
#include "kernel/file_system/file_system.h"
#include "kernel/multitasking/multitasking.h"

#include "common.h"

VirtualAddressSpace* syscallAddressSpace;
uint64_t* syscallStack;

uint64_t(*syscallTable[SYS_AMOUNT])(void);
uint64_t syscallAmount = SYS_AMOUNT;

uint64_t syscall_yield()
{   
    tty_print("Syscall yield test\n\r");

    Task* nextTask = load_next_task();

    return nextTask;
}

uint64_t syscall_exit()
{   
    tty_print("Syscall exit test\n\r");
    return 0;
}

void syscall_init(VirtualAddressSpace* addressSpace, uint64_t* stack)
{
    syscallAddressSpace = addressSpace;
    syscallStack = stack;

    syscallTable[SYS_YIELD] = syscall_yield;
    syscallTable[SYS_EXIT] = syscall_exit;
}
