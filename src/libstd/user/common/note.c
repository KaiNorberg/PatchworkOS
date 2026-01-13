#include <sys/proc.h>
#include <user/common/note.h>
#include <user/common/syscalls.h>

#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static _Atomic(atnotify_func_t) noteHandlers[_NOTE_MAX_HANDLERS] = {ATOMIC_VAR_INIT(NULL)};
static _Atomic(sighandler_t) signalHandlers[SIGMAX] = {ATOMIC_VAR_INIT(SIG_DFL)};

static void _signal_invoke(int sig, const char* note)
{
    sighandler_t handler = atomic_load(&signalHandlers[sig]);
    if (handler == SIG_IGN)
    {
        return;
    }

    if (handler == SIG_DFL)
    {
        proc_exit(note);
    }

    handler(sig);
}

_NORETURN static void _note_kernel_handler(char* note)
{
    for (uint64_t i = 0; i < _NOTE_MAX_HANDLERS; i++)
    {
        atnotify_func_t func = atomic_load(&noteHandlers[i]);
        if (func != NULL)
        {
            uint64_t result = func(note);
            if (result == ERR)
            {
                proc_exit(note);
            }
        }
    }

    if (wordcmp(note, "divbyzero") == 0)
    {
        _signal_invoke(SIGFPE, note);
    }
    else if (wordcmp(note, "illegal") == 0)
    {
        _signal_invoke(SIGILL, note);
    }
    else if (wordcmp(note, "interrupt") == 0)
    {
        _signal_invoke(SIGINT, note);
    }
    else if (wordcmp(note, "pagefault") == 0 || wordcmp(note, "segfault") == 0)
    {
        _signal_invoke(SIGSEGV, note);
    }
    else if (wordcmp(note, "terminate") == 0)
    {
        _signal_invoke(SIGTERM, note);
    }

    noted();
}

void _note_init(void)
{
    if (notify(_note_kernel_handler) == ERR)
    {
        proc_exit("notify failed");
    }
}

uint64_t _note_handler_add(atnotify_func_t func)
{
    for (uint64_t i = 0; i < _NOTE_MAX_HANDLERS; i++)
    {
        atnotify_func_t expected = NULL;
        if (atomic_compare_exchange_strong(&noteHandlers[i], &expected, func))
        {
            return 0;
        }
    }

    return ERR;
}

void _note_handler_remove(atnotify_func_t func)
{
    for (uint64_t i = 0; i < _NOTE_MAX_HANDLERS; i++)
    {
        atnotify_func_t expected = func;
        if (atomic_compare_exchange_strong(&noteHandlers[i], &expected, NULL))
        {
            return;
        }
    }
}

int _signal_raise(int sig)
{
    if (sig <= 0 || sig >= SIGMAX)
    {
        return -1;
    }

    _signal_invoke(sig, F("signal %d raised", sig));
    return 0;
}

sighandler_t _signal_handler_add(int sig, sighandler_t func)
{
    if (sig <= 0 || sig >= SIGMAX)
    {
        return SIG_ERR;
    }

    sighandler_t previous = atomic_exchange(&signalHandlers[sig], func);
    return previous;
}

void _signal_handler_remove(int sig, sighandler_t func)
{
    if (sig <= 0 || sig >= SIGMAX)
    {
        return;
    }

    sighandler_t expected = func;
    atomic_compare_exchange_strong(&signalHandlers[sig], &expected, SIG_DFL);
}