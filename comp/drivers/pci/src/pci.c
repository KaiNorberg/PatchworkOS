#include <kernel/module/module.h>

#include <sys/defs.h>

status_t _module_procedure(const module_event_t* event)
{
    UNUSED(event);
    return OK;
}