#pragma once

#include <stdint.h>

#include <common/boot_info/boot_info.h>

#define PAGE_SIZE 0x1000

#define SIZE_IN_PAGES(size) (((size) / PAGE_SIZE) + 1)
#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

#define QWORD_INDEX(address) (((uint64_t)address / PAGE_SIZE) / 64)
#define BIT_INDEX(address) (((uint64_t)address / PAGE_SIZE) % 64)

void pmm_init(EfiMemoryMap* memoryMap);

void* pmm_allocate();

void* pmm_allocate_amount(uint64_t amount);

uint8_t pmm_is_reserved(void* address);

void pmm_reserve_page(void* address);

void pmm_free_page(void* address);

void pmm_reserve_pages(void* address, uint64_t count);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount();

uint64_t pmm_free_amount();

uint64_t pmm_reserved_amount();

uint64_t pmm_usable_amount();

uint64_t pmm_unusable_amount();