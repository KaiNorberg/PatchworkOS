#include "../platform.h"
#include "common/print.h"
#include "common/thread.h"

#include <sys/io.h>
#include <sys/proc.h>

static fd_t zeroResource;

void _PlatformInit(void)
{
    zeroResource = open("sys:/zero");
}

void* _PlatformPageAlloc(uint64_t amount)
{
    return valloc(NULL, amount * PAGE_SIZE, PROT_READ | PROT_WRITE);
}

int* _PlatformErrnoFunc(void)
{
    return &_ThreadById(gettid())->err;
}

// TODO: Implement streams!
int _PlatformVprintf(const char* _RESTRICT format, va_list args)
{
    void put_func(char chr, void* context)
    {
        write(STDOUT_FILENO, &chr, 1);
    }

    return _Print(put_func, NULL, format, args);
}