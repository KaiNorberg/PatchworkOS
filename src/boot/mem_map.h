#pragma once

#include <stdint.h>

#include <efi.h>
#include <efilib.h>

#include <boot/boot_info.h>

void mem_map_init(efi_mem_map_t* memoryMap);

void mem_map_deinit(efi_mem_map_t* memoryMap);
