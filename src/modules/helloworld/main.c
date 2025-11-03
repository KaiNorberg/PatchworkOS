#include <kernel/log/log.h>
#include <kernel/module/module.h>

void hello_world(void); // Defined in the dependency module

uint64_t module_procedure(module_event_t* event)
{
    (void)event;

    LOG_INFO("Hello world module loaded!\n");
    hello_world();
    return 0;
}

MODULE_INFO("Hello World", "Kai Norberg", "A simple hello world module for testing", "1.0.0", "MIT");
