#include <common/boot_info.h>

#include <string.h>

#include "debug.h"
#include "defs.h"
#include "kernel.h"
#include "sched.h"
#include "smp.h"
#include "time.h"
#include "tty.h"
#include "utils.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_clear();

    sched_spawn("home:/bin/shell.elf");

    // Exit init thread
    sched_thread_exit();
}