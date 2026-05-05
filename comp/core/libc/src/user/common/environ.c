#include "environ.h"

char** environ;

void _environ_set(char** envp)
{
    environ = envp;
}
