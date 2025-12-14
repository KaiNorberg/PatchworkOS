#include <signal.h>

#include <user/common/note.h>

sighandler_t signal(int sig, sighandler_t func)
{
    return _signal_handler_add(sig, func);
}