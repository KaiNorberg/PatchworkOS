#pragma once

#include "defs.h"

#include <bootloader/boot_info.h>

#define DWM_TARGET_DELTA (SEC / 60)

void dwm_init(gop_buffer_t* gopBuffer);

void dwm_start(void);

void dwm_redraw(void);

void dwm_update_client_rect(void);
