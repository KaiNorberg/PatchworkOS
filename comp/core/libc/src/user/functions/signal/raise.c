#include <user/common/note.h>

#include <signal.h>

int raise(int sig)
{
    return _signal_raise(sig);
}