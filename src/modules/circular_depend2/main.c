#include <kernel/log/log.h>
#include <kernel/module/module.h>

void circular_depend1(void);

void circular_depend2(void)
{
    LOG_INFO("Circular depend 2 function called!\n");
}

uint64_t _module_procedure(module_event_t* event)
{
    (void)event;

    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        LOG_INFO("Circular depend 2 load!\n");
        circular_depend1();
        break;
    case MODULE_EVENT_UNLOAD:
        LOG_INFO("Circular depend 2 unload!\n");
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Circular Depend2", "Kai Norberg", "A simple circular dependency module for testing", "1.0.0", "MIT");
