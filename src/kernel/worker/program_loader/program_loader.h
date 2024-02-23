#pragma once

#include "worker/process/process.h"
#include "vfs/vfs.h"

#include <lib-asym.h>

Status load_program(Process* process, File* file);
