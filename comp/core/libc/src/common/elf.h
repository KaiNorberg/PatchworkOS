#pragma once

#include <errno.h>
#include <libc/elf.h>
#include <stddef.h>
#include <string.h>

#ifdef _KERNEL_
#include <kernel/log/log.h>
#endif