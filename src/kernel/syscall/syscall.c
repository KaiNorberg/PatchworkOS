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

    switch(frame->rax)
    {
    case SYS_TEST:
    {
        tty_print("Syscall test, rdi = "); tty_printi(frame->rdi); tty_print("!\r");

        out = 0;
    }
    break;
    case SYS_YIELD:
    {
        context_save(multitasking_get_running_task()->context, frame);
        multitasking_schedule();    
        context_load(multitasking_get_running_task()->context, frame);

        out = 0;
    }
    break;
    case SYS_EXIT:
    {
        Task* oldTask = multitasking_get_running_task();

        multitasking_schedule();

        multitasking_free(oldTask);

        context_load(multitasking_get_running_task()->context, frame);

        out = 0;
    }
    break;
    default:
    {
        out = -1;
    }
    break;
    }

    frame->rax = out;
}
