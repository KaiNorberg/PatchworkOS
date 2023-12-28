#include "multiprocessing.h"

#include "tty/tty.h"

uint64_t cpuAmount = 0;

void multiprocessing_init(void* entry)
{    
    tty_start_message("Multiprocessing initializing");

    cpuAmount = 0;

    for (uint64_t i = 0; i < multiprocessing_get_cpu_amount(); i++)
    {
        
    }    
    
    tty_end_message(TTY_MESSAGE_OK);

    tty_print("Cpu Amount: "); tty_printi(multiprocessing_get_cpu_amount()); tty_print("\n\r");
}

uint8_t multiprocessing_get_cpu_amount()
{
    return cpuAmount;
}