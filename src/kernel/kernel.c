#include "kernel.h"

#include "acpi.h"
#include "apic.h"
#include "const.h"
#include "dwm.h"
#include "gdt.h"
#include "hpet.h"

#include "idt.h"
#include "log.h"
#include "madt.h"
#include "pic.h"
#include "pmm.h"
#include "ps2.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "ramfs.h"
#include "regs.h"
#include "sched.h"
#include "simd.h"
#include "smp.h"
#include "sysfs.h"
#include "time.h"
#include "vfs.h"
#include "vmm.h"
#include <common/boot_info.h>

#include <stdlib_internal/init.h>

void kernel_init(boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();

    log_init();

    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap, &bootInfo->gopBuffer);

    log_enable_screen(&bootInfo->gopBuffer);

    _StdInit();

    acpi_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();

    time_init();
    log_enable_time();

    pic_init();

    smp_init();
    kernel_cpu_init();

    sched_start();

    vfs_init();
    sysfs_init();
    ramfs_init(bootInfo->ramRoot);

    ps2_init();
    ps2_keyboard_init();
    ps2_mouse_init();

    const_init();
    dwm_init(&bootInfo->gopBuffer);

    pmm_free_type(EFI_MEM_BOOT_INFO);

    dwm_start();
    log_disable_screen();
}

void kernel_cpu_init(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);
    gdt_load_tss(&cpu->tss);

    lapic_init();
    simd_init();

    vmm_cpu_init();

    log_print("CPU %d: initialized", (uint64_t)cpu->id);
}
