#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#elif defined(_BOOT_)
#include <efi.h>

#endif

void _assert_99(const char* const message1, const char* const function, const char* const message2)
{
#ifdef _KERNEL_
    panic(NULL, "%s %s %s %s", message1, function, message2, errno != 0 ? strerror(errno) : "errno not set");
#elif defined(_BOOT_)
    Print(L"%a %a %a\n", message1, function, message2);
    for (;;)
    {
        __asm__("cli; hlt");
    }
#else
    fputs(message1, stderr);
    fputs(function, stderr);
    fputs(message2, stderr);
    abort();
#endif
}

void _assert_89(const char* const message)
{
#ifdef _KERNEL_
    panic(NULL, message);
#elif defined(_BOOT_)
    Print(L"%a\n", message);
    for (;;)
    {
        __asm__("cli; hlt");
    }
#else
    fputs(message, stderr);
    abort();
#endif
}
