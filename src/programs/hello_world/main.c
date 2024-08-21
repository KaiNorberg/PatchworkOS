#include <sys/io.h>

int main(void)
{
    write(STDOUT_FILENO, "Hello, World!\n", 14);
    return 0;
}
