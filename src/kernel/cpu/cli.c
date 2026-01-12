#include <kernel/cpu/cli.h>

#include <sys/proc.h>
#include <sys/defs.h>

cli_t _cli[CPU_MAX] ALIGNED(PAGE_SIZE) = {0};