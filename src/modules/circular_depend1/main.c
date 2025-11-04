#include <kernel/log/log.h>
#include <kernel/module/module.h>

void circular_depend1(void)
{
    LOG_INFO("Circular depend 1 function called!\n");
}

void circular_depend2(void);

uint64_t _module_procedure(module_event_t* event)
{
    (void)event;

    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Circular depend 1 load!\n");
        circular_depend2();
        break;
    case MODULE_EVENT_UNLOAD:
        LOG_INFO("Circular depend 1 unload!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Circular Depend1", "Kai Norberg", "A simple circular dependency module for testing", "1.0.0", "MIT", "");
