#include <time.h>

char* ctime(const time_t* timer)
{
    struct tm* tmp = localtime(timer);
    return tmp ? asctime(tmp) : NULL;
}