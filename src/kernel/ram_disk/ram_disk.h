#pragma once

#include <common/boot_info/boot_info.h>
#include <stdint.h>

#include "list/list.h"

/*
typedef struct
{
	char name[32];
	void* data;
	uint64_t size;
} RamFile;

typedef struct
{
	char name[32];
	List* files;
	List* children;
} RamDirectory;
*/

void ram_disk_init(RamDirectory* root);