#include "const.h"

#include "log.h"
#include "sysfs.h"
#include "vfs.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static uint64_t const_null_read(file_t* file, void* buffer, uint64_t count)
{
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, uint64_t count)
{
    return 0;
}

static file_ops_t nullOps = {
    .read = const_null_read,
    .write = const_null_write,
};

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(nullObjOps, &nullOps);

void const_init(void)
{
    ASSERT_PANIC(sysobj_new("/", "null", &nullObjOps, NULL) != NULL);
}
