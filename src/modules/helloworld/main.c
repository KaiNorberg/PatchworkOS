#include <kernel/log/log.h>
#include <kernel/module/module.h>

void hello_world1(void); // Defined in the dependency module

uint64_t _module_procedure(module_event_t* event)
{
    (void)event;

    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Hello world load!\n");
        hello_world1();
        break;
    case MODULE_EVENT_UNLOAD:
        LOG_INFO("Hello world unload!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Hello World", "Kai Norberg", "A simple hello world module for testing", "1.0.0", "MIT");
