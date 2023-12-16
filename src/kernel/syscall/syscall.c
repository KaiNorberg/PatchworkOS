#include "syscall.h"

#include "tty/tty.h"
#include "file_system/file_system.h"
#include "multitasking/multitasking.h"
#include "context/context.h"
#include "string/string.h"
#include "debug/debug.h"

#include "kernel/kernel.h"

#include "../common.h"

void syscall_init()
{

}

void syscall_handler(InterruptStackFrame* frame)
{    
    uint64_t out;

    switch(frame->RAX)
    {
    case SYS_TEST:
    {
        tty_print("Syscall test, rdi = "); tty_printi(frame->RDI); tty_print("!\r");

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        context_save(multitasking_get_running_task()->Context, frame);
        multitasking_schedule();    
        context_load(multitasking_get_running_task()->Context, frame);

        out = 0;
    }
    break;
    case SYS_EXIT:
    {
        Task* oldTask = multitasking_get_running_task();

        multitasking_schedule();

        multitasking_free(oldTask);

        context_load(multitasking_get_running_task()->Context, frame);

        out = 0;
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    frame->RAX = out;
}
