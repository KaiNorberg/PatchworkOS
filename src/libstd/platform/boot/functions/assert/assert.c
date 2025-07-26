#include <assert.h>

#include <gnu-efi/inc/efi.h>
#include <gnu-efi/inc/efilib.h>

void _assert_99(const char* const message1, const char* const function, const char* const message2)
{
    Print(L"%a %a %a\n", message1, function, message2);
    Exit(EFI_ABORTED, 0, NULL);
}

void _assert_89(const char* const message)
{
    Print(L"%a", message);
    Exit(EFI_ABORTED, 0, NULL);
}
