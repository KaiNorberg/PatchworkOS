#include "kernel.h"
#include "loader.h"
#include "log.h"
#include "ring.h"
#include "sched.h"

#include <bootloader/boot_info.h>
#include <stdio.h>
#include <string.h>

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    const char* argv[] = {"home:/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MIN + 1);
    ASSERT_PANIC(initThread != NULL);

    sched_push(initThread);

    // Exit boot thread
    sched_thread_exit();
}
