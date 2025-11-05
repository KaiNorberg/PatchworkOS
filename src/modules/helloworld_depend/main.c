#include <kernel/log/log.h>
#include <kernel/module/module.h>

void hello_world2(void);

void hello_world1(void)
{
    LOG_INFO("Hello world from the depend1 module!\n");
    hello_world2();
}

uint64_t _module_procedure(module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Hello world depend1 load!\n");
        break;
    case MODULE_EVENT_UNLOAD:
        LOG_INFO("Hello world depend1 unload!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Hello World Depend1", "Kai Norberg", "A simple module used to test module dependencies", "1.0.0", "MIT", "");
