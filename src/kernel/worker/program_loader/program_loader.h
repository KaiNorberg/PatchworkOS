#pragma once

#include <lib-asym.h>

#include "worker/process/process.h"
#include "vfs/vfs.h"

Status load_program(Process* process, File* file);
