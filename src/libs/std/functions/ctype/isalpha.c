#include <ctype.h>

int isalpha(int ch)
{
    return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) ? 1 : 0;
}