#include <kernel/cpu/port.h>
#include <kernel/sync/lock.h>

#include <kernel/log/log.h>

#include <stdbool.h>
#include <stddef.h>
#include <sys/bitmap.h>
#include <sys/list.h>

static BITMAP_CREATE(ports, PORT_MAX + 1);
static lock_t lock = LOCK_CREATE();

status_t port_reserve(port_t* out, port_t minBase, port_t maxBase, uint64_t alignment, uint64_t length,
    const char* owner)
{
    UNUSED(owner);

    if (out == NULL || length == 0 || minBase > maxBase)
    {
        return ERR(PORT, INVAL);
    }
    LOCK_SCOPE(&lock);

    if (maxBase + length < maxBase || maxBase + length > ports.length)
    {
        return ERR(PORT, TOOBIG);
    }

    uint64_t base = bitmap_find_clear_region_and_set(&ports, minBase, maxBase + length, length, alignment);
    if (base == ports.length)
    {
        return ERR(PORT, NOSPACE);
    }

    *out = (port_t)base;
    return 0;
}

void port_release(port_t base, uint64_t length)
{
    if (length == 0 || base + length < base || base + length > ports.length)
    {
        return;
    }
    LOCK_SCOPE(&lock);

    bitmap_clear_range(&ports, base, base + length);
}