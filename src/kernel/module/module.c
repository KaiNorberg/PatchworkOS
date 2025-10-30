#include <kernel/module/module.h>

#include <kernel/sync/lock.h>

static list_t modules = LIST_CREATE(modules);
static lock_t modulesLock = LOCK_CREATE;

void module_init(void)
{
}

uint64_t module_event(module_event_t* event)
{
    (void)event;
    return 0;
}
