#pragma once

#include "defs/defs.h"

extern NORETURN void loader_jump_to_user_space(void* rsp, void* rip);

NORETURN void loader_entry();