#pragma once

#include "defs.h"
#include "list.h"
#include "lock.h"
#include "message.h"
#include "window.h"

#include <common/boot_info.h>

void dwm_init(gop_buffer_t* gopBuffer);

void dwm_start(void);

void dwm_redraw(void);
