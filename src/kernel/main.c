#include <common/boot_info.h>

#include "defs.h"
#include "kernel.h"
#include "sched.h"

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    sched_spawn("home:/bin/shell.elf");
    sched_spawn("home:/bin/calculator.elf");

    // Exit init thread
    sched_thread_exit();
}
