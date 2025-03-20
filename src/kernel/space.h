#pragma once

#include "defs.h"
#include "lock.h"
#include "pml.h"

typedef struct
{
    pml_t* pml;
    uintptr_t freeAddress;
    lock_t lock;
} space_t;

void space_init(space_t* space);

void space_deinit(space_t* space);

void space_load(space_t* space);
