#include "kernel.h"
#include "loader.h"
#include "log.h"
#include "ring.h"
#include "sched.h"

#include <bootloader/boot_info.h>
#include <string.h>
#include <stdio.h>

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    printf("kernel: shell spawn");

    const char* argv[] = {"home:/bin/shell", NULL};
    thread_t* shell = loader_spawn(argv, PRIORITY_MIN + 1);
    ASSERT_PANIC(shell != NULL, "Failed to spawn shell");

    sched_push(shell);

    // Exit init thread
    sched_thread_exit();
}
