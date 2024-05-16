#include <common/boot_info.h>

#include <string.h>

#include "tty.h"
#include "smp.h"
#include "time.h"
#include "debug.h"
#include "kernel.h"
#include "sched.h"
#include "defs.h"
#include "utils.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_clear();

    sched_spawn("B:/programs/shell.elf");

    //Exit init thread
    sched_thread_exit();
}