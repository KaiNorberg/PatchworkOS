#include <bootloader/boot_info.h>
#include <stdlib.h>

#include "defs.h"
#include "kernel.h"
#include "log.h"
#include "process.h"
#include "sched.h"

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    log_print("kernel: shell spawn");
    if (sched_spawn("home:/bin/shell.elf", THREAD_PRIORITY_MIN + 1) == ERR)
    {
        log_panic(NULL, "Failed to spawn shell");
    }

    // Exit init thread
    sched_thread_exit();
}
