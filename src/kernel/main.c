#include <bootloader/boot_info.h>
#include <string.h>

#include "kernel.h"
#include "log.h"
#include "thread.h"
#include "sched.h"
#include "loader.h"

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    log_print("kernel: shell spawn");

    const char* argv[] = {"home:/bin/shell", NULL};
    thread_t* shell = loader_spawn(argv, PRIORITY_MIN + 1);
    LOG_ASSERT(shell != NULL, "Failed to spawn shell");

    sched_push(shell);

    // Exit init thread
    sched_thread_exit();
}
