#pragma once

#include "window.h"

#include <sys/io.h>
#include <sys/list.h>

void dwm_init(void);

void dwm_deinit(void);

void dwm_loop(void);
