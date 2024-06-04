#ifndef _SYS_WIN_H
#define _SYS_WIN_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/ERR.h"

#define WIN_DEFAULT ERR

typedef struct win_info 
{
    uint64_t width;
    uint64_t height;
    uint64_t x;
    uint64_t y;
} win_info_t;

#if defined(__cplusplus)
}
#endif

#endif