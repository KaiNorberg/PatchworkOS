#include "debug.h"

#include "kernel/tty/tty.h"

#include "kernel/heap/heap.h"
#include "kernel/page_allocator/page_allocator.h"

#include "common.h"

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

void debug_error(const char* message)
{
    uint64_t randomNumber = 0;

    Pixel black;
    black.A = 255;
    black.R = 0;
    black.G = 0;
    black.B = 0;

    Pixel white;
    white.A = 255;
    white.R = 255;
    white.G = 255;
    white.B = 255;

    Pixel green;
    green.A = 255;
    green.R = 152;
    green.G = 195;
    green.B = 121;

    Pixel red;
    red.A = 255;
    red.R = 224;
    red.G = 108;
    red.B = 117;

    asm volatile("cli");

    uint64_t scale = 3;

    Point startPoint;
    startPoint.X = 100;
    startPoint.Y = 50;

    tty_set_scale(scale);

    tty_clear();

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.X, startPoint.Y);

    tty_print("KERNEL PANIC!\n\r");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 1 * scale);
    tty_print("// ");
    tty_print(errorJokes[randomNumber]);

    tty_set_background(black);
    tty_set_foreground(red);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 3 * scale);
    tty_print("ERROR: ");
    tty_print("\"");
    tty_print(message);
    tty_print("\"");

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 5 * scale);
    tty_print("OS_VERSION = ");
    tty_print(OS_VERSION);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 7 * scale);
    tty_print("Time: ");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 8 * scale);
    tty_print("Ticks = ");
    //tty_print(STL::ToString(PIT::Ticks));

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 9 * scale);
    tty_print("Current Time = ");
    //tty_print(STL::ToString(RTC::GetHour()));
    tty_print(":");
    //tty_print(STL::ToString(RTC::GetMinute()));
    tty_print(":");
    //tty_print(STL::ToString(RTC::GetSecond()));

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 11 * scale);
    tty_print("Memory: ");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 12 * scale);
    tty_print("Used Heap = ");
    tty_printi(heap_reserved_size() / 0x1000);
    tty_print(" KB");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 13 * scale);
    tty_print("Free Heap = ");
    tty_printi(heap_free_size() / 0x1000);
    tty_print(" KB");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 14 * scale);
    tty_print("Locked Pages = ");
    tty_printi(page_allocator_get_locked_amount());

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 15 * scale);
    tty_print("Unlocked Pages = ");
    tty_printi(page_allocator_get_unlocked_amount());

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 17 * scale);
    tty_print("System Halted!");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 18 * scale);
    tty_print("Please manually reboot your machine.");

    while (1)
    {
        asm volatile("hlt");
    }
}