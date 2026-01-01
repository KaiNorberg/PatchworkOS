#include <kernel/module/module.h>

#include <sys/defs.h>

uint64_t _module_procedure(const module_event_t* event)
{
    UNUSED(event);
    return 0;
}

MODULE_INFO("APCI PCI Driver", "Kai Norberg", "An ACPI PCI host bridge driver", OS_VERSION, "MIT", "");