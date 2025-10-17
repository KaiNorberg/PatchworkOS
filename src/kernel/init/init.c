#include "acpi/acpi.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/simd.h"
#include "cpu/smp.h"
#include "cpu/syscalls.h"
#include "drivers/apic.h"
#include "drivers/const.h"
#include "drivers/fb/gop.h"
#include "drivers/ps2/ps2.h"
#include "fs/ramfs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "gnu-efi/inc/efidef.h"
#include "ipc/pipe.h"
#include "ipc/shmem.h"
#include "log/log.h"
#include "log/log_file.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "net/net.h"
#include "proc/process.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"

#include <boot/boot_info.h>
#include <libstd/_internal/init.h>
#include <strings.h>

static const boot_info_t* bootInfo;

void init_early(const boot_info_t* info)
{
    bootInfo = info;

    gdt_init();
    gdt_cpu_load();

    idt_init();
    idt_cpu_load();

    smp_bootstrap_init();
    gdt_cpu_tss_load(&smp_self_unsafe()->tss);

    log_init(&bootInfo->gop);

    pmm_init(&bootInfo->memory.map);
    vmm_init(&bootInfo->memory, &bootInfo->gop, &bootInfo->kernel);
    heap_init();

    simd_cpu_init();

    syscall_table_init();
    syscalls_cpu_init();

    sched_init();
    timer_init();
    wait_init();

    LOG_DEBUG("early init done, jumping to boot thread\n");
}

void init_other_cpu(cpuid_t id)
{
    msr_write(MSR_CPU_ID, id);

    gdt_cpu_load();
    idt_cpu_load();

    gdt_cpu_tss_load(&smp_self_unsafe()->tss);

    lapic_cpu_init();
    simd_cpu_init();

    vmm_cpu_init();
    syscalls_cpu_init();
    timer_cpu_init();
}

static void init_free_loader_data(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiLoaderData)
        {
            pmm_free_pages((void*)desc->VirtualStart, desc->NumberOfPages);
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                ((uintptr_t)desc->VirtualStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }
}

static void init_finalize(void)
{
    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);

    vmm_map_bootloader_lower_half(bootThread);
    panic_symbols_init(&bootInfo->kernel);

    _std_init();

    vfs_init();
    ramfs_init(&bootInfo->disk);
    sysfs_init();

    acpi_init(bootInfo->rsdp, &bootInfo->memory.map);

    lapic_init();
    lapic_cpu_init();
    ioapic_all_init();

    timer_cpu_init();

    log_file_expose();
    process_procfs_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    shmem_init();
    gop_init(&bootInfo->gop);
    statistics_init();

    smp_others_init();

    init_free_loader_data(&bootInfo->memory.map);
    vmm_unmap_bootloader_lower_half(bootThread);

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_reserved_amount() * PAGE_SIZE / 1024);
}

static inline void init_process_spawn(void)
{
    LOG_INFO("spawning init process\n");
    const char* argv[] = {"/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MAX_USER - 2, NULL);
    if (initThread == NULL)
    {
        panic(NULL, "Failed to spawn init process");
    }

    // Set klog as stdout for init process
    file_t* klog = vfs_open(PATHNAME("/dev/klog"));
    if (klog == NULL)
    {
        panic(NULL, "Failed to open klog");
    }
    if (vfs_ctx_openas(&initThread->process->vfsCtx, STDOUT_FILENO, klog) == ERR)
    {
        panic(NULL, "Failed to set klog as stdout for init process");
    }
    ref_dec(klog);

    sched_push(initThread, NULL);
}

void kmain(void)
{
    // The stack pointer is expected to be somewhere near the top.
    assert(rsp_read() > VMM_KERNEL_STACKS_MAX - 2 * PAGE_SIZE);

    LOG_DEBUG("kmain entered\n");

    init_finalize();

    init_process_spawn();

    LOG_INFO("done with boot thread\n");
    sched_done_with_boot_thread();
}
