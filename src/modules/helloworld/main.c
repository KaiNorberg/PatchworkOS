#include <kernel/log/log.h>
#include <kernel/module/module.h>

uint64_t _module_procedure(module_event_t* event)
{
    (void)event;

    LOG_DEBUG("Hello world from a module!\n");
    return 0;
}

MODULE_INFO("hello world", "Kai Norberg", "A simple hello world module for testing", "1.0.0", "MIT");

// The used HID is the APIC device, used to test that this module will be loaded, since APIC is always present.
MODULE_ACPI_HIDS("PNP0F0C");
