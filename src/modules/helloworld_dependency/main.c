#include <kernel/log/log.h>
#include <kernel/module/module.h>

void hello_world(void)
{
    LOG_INFO("Hello world from the dependency module!\n");
}

uint64_t module_procedure(module_event_t* event)
{
    (void)event;

    LOG_INFO("Hello world dependency module loaded!\n");
    return 0;
}

MODULE_INFO("Hello World Dependency", "Kai Norberg",
    "A simple module defining a hello_world() function that is then called in the Hello World module, used to test "
    "dependency loading",
    "1.0.0", "MIT");
