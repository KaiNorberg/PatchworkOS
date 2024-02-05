#include <libc/ctype.h>

int isdigit(int ch)
{
    return (ch >= '0' && ch <= '9') ? 1 : 0;
}