#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/win.h>

int main(void)
{
    return EXIT_SUCCESS;
}

/*int main(void)
{
    fd_t window = open("sys:/srv/win");
    if (window == ERR)
    {
        return EXIT_FAILURE;
    }

    win_init_t initInfo;
    write(window, &initInfo, sizeof(win_init_t));

    void* surface = mmap(window, NULL, initInfo.x * initInfo.y, PROT_READ | PROT_WRITE);

    //Event loop
    while (1)
    {
        win_event_t event;
        read(window, &event, sizeof(win_event_t));

        //Do stuff...
        if (event.type == WIN_EVENT_KEYBOARD)
        {

        }
    }

    close(window);

    return EXIT_SUCCESS;
}

int main_wrapped(void)
{
    win_init_t initInfo;
    win_t window = win_init(&initInfo);

    //Event loop
    while (1)
    {
        win_poll_events(window);

        win_pixel(window, 0, 0, 0xFFFFFF);

        win_rectangle(window, 10, 10, 100, 100, 0x000000);

        //Do stuff...
    }

    win_cleanup(window);

    return EXIT_SUCCESS;
}*/