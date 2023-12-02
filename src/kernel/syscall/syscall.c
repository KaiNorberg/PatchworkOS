#include "syscall.h"

#include "kernel/tty/tty.h"
#include "kernel/file_system/file_system.h"
#include "kernel/multitasking/multitasking.h"

#include "common.h"

VirtualAddressSpace* syscallAddressSpace;
uint64_t* syscallStack;

void syscall_init(VirtualAddressSpace* addressSpace, uint64_t* stack)
{
    syscallAddressSpace = addressSpace;
    syscallStack = stack;
}

uint64_t syscall_handler()
{   
    tty_print("Syscall test!\n\r");

    return 0;
}

