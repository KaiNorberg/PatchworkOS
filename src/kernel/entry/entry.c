#include <common/boot_info/boot_info.h>

#include <string.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "debug/debug.h"
#include "kernel/kernel.h"
#include "sched/sched.h"
#include "defs/defs.h"
#include "utils/utils.h"
#include "vfs/utils/utils.h"

#include "heap/heap.h"

void pmm()
{
    for (uint64_t i = 0; i < 500000; i++)
    {
        pmm_allocate();
    }
}

void heap()
{
    for (uint64_t i = 0; i < 5000; i++)
    {
        kmalloc(1000);
    }
}

#define BUFFER_SIZE 32

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_acquire();
    tty_clear();
    tty_set_row(smp_cpu_amount() + 2);
    tty_release();

    tty_print("RESERVED: ");
    tty_printi((pmm_reserved_amount() * PAGE_SIZE) / 1024);
    tty_print(" KB\n");

    //BENCHMARK(pmm);
    //BENCHMARK(heap);

    /*tty_print("CHDIR: ");
    if (vfs_chdir("B:/programs") == ERR)
    {
        tty_print(strerror(sched_thread()->error));
    }
    else
    {
        tty_print("SUCCESS");
    }
    tty_print("\n");

    uint64_t fd = vfs_open("/test1/test2/test3/test.txt");
    tty_print("OPEN: ");
    if (fd == ERR)
    {
        tty_print(strerror(sched_thread()->error));
    }
    else
    {
        tty_print("SUCCESS");
    }
    tty_print("\n");

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    tty_print("READ: ");
    if (vfs_read(fd, buffer, BUFFER_SIZE - 1) == ERR)
    {
        tty_print(strerror(sched_thread()->error));
    }
    else
    {
        tty_print(buffer);
    }
    tty_print("\n");

    tty_print("CLOSE: ");
    if (vfs_close(fd) == ERR)
    {
        tty_print(strerror(sched_thread()->error));
    }
    else
    {
        tty_print("SUCCESS");
    }
    tty_print("\n");*/

    for (uint64_t i = 0; i < 1; i++)
    {
        sched_spawn("B:/programs/parent.elf");
    }

    //Exit init thread
    sched_thread_exit();
}
