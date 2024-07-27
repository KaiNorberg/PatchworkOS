#pragma once

#include <sys/win.h>

void shell_init(void);

void shell_loop(void);

void shell_push(win_t* window);
