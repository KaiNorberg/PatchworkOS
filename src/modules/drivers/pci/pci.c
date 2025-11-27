#include <kernel/module/module.h>

#include <kernel/defs.h>

uint64_t _module_procedure(const module_event_t* event)
{
    (void)event;
    return 0;
}

MODULE_INFO("APCI PCI Driver", "Kai Norberg", "An ACPI PCI host bridge driver", OS_VERSION, "MIT",
    "");