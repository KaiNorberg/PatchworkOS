#include <kernel/log/log.h>
#include <kernel/module/module.h>

void hello_world2(void)
{
    LOG_INFO("Hello world from the depend2 module!\n");
}

uint64_t _module_procedure(module_event_t* event)
{
    (void)event;

    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Hello world depend2 load!\n");
        break;
    case MODULE_EVENT_UNLOAD:
        LOG_INFO("Hello world depend2 unload!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Hello World Depend2", "Kai Norberg", "Another simple module used to test module dependencies", "1.0.0",
    "MIT", "");
