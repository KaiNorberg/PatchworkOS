#pragma once

#include <sys/io.h>
#include <threads.h>

extern ioring_t _stdIoring;
extern mtx_t _stdIoringMtx;

void _io_init(void);