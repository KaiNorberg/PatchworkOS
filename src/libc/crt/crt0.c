#include "libc/include/stdlib.h"

int main(int argc, char* argv[]);

void _start()
{
    int status = main(0, 0);
    exit(status);
}