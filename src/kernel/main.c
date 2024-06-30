#include <bootloader/boot_info.h>

#include "defs.h"
#include "log.h"
#include "kernel.h"
#include "process.h"
#include "sched.h"
#include "vfs.h"

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    sched_spawn("home:/bin/shell", THREAD_PRIORITY_MIN + 1);

    // Exit init thread
    sched_thread_exit();
}
