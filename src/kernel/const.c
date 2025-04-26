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


SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(nullOps, (file_ops_t){
    .read = const_null_read,
    .write = const_null_write,
});

void const_init(void)
{
    ASSERT_PANIC(sysobj_new("/", "null", &nullOps, NULL) != NULL);
}
