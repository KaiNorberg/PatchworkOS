#include "debug.h"

#include "tty/tty.h"

#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "time/time.h"

#include "../common.h"

//Jokes provided by skift-os https://github.com/skift-org/skift/tree/main :)

const char* errorJokes[] = 
{
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!!!",
    "Witty comment unavailable :(",
    "Surprise! Haha. Well, this is awkward.",
    "Oh - I know what I did wrong!",
    "Uh... Did I do that?",
    "Oops.",
    "DON'T PANIC!",
    "Greenpeace free'd the mallocs \\o/",
    "Typo in the code.",
    "System consumed all the paper for paging!",
    "I'm tired of this ;_;",
    "PC LOAD LETTER",
    "Abort, Retry, Fail?",
    "Everything's going to plan. No, really, that was supposed to happen.",
    "My bad.",
    "Quite honestly, I wouldn't worry myself about that.",
    "This doesn't make any sense!",
    "It's not a good surprise...",
    "Don't do that.",
    "Layer 8 problem detected.",
    "PEBCAK detected."        
};

void debug_panic(const char* message)
{
    uint64_t randomNumber = 0;

    Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel white;
    white.a = 255;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    /*Pixel green;
    green.a = 255;
    green.r = 152;
    green.g = 195;
    green.b = 121;*/

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    asm volatile("cli");

    uint64_t scale = 3;

    Point startPoint;
    startPoint.x = 100;
    startPoint.y = 50;

    tty_set_scale(scale);

    tty_clear();

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y);

    tty_print("KERNEL PANIC!\n\r");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 1 * scale);
    tty_print("// ");
    tty_print(errorJokes[randomNumber]);

    tty_set_background(black);
    tty_set_foreground(red);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 3 * scale);
    tty_print("ERROR: ");
    tty_print("\"");
    tty_print(message);
    tty_print("\"");

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 5 * scale);
    tty_print("OS_VERSION = ");
    tty_print(OS_VERSION);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 7 * scale);
    tty_print("Time: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 8 * scale);
    tty_print("Ticks = ");
    tty_printi(time_get_tick());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 9 * scale);
    tty_print("Current Time = ");
    //tty_print(STL::ToString(RTC::GetHour()));
    tty_print(":");
    //tty_print(STL::ToString(RTC::GetMinute()));
    tty_print(":");
    //tty_print(STL::ToString(RTC::GetSecond()));

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 11 * scale);
    tty_print("Memory: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 12 * scale);
    tty_print("Used Heap = ");
    tty_printi(heap_reserved_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 13 * scale);
    tty_print("Free Heap = ");
    tty_printi(heap_free_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 14 * scale);
    tty_print("Locked Pages = ");
    tty_printi(page_allocator_get_locked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 15 * scale);
    tty_print("Unlocked Pages = ");
    tty_printi(page_allocator_get_unlocked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 17 * scale);
    tty_print("Please manually reboot your machine.");

    while (1)
    {
        asm volatile("hlt");
    }
}