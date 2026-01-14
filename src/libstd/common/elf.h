#pragma once

#include <errno.h>
#include <stddef.h>
#include <sys/elf.h>

// Makes sure both the kernel and bootloader can use the elf functions 
// as the bootloader will define its own string functions before including this file
#ifndef _BOOT_
#include <string.h>
#endif

#ifdef _KERNEL_
#include <kernel/log/log.h>
#endif