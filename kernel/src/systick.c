/**
* @file
*
* @brief
*
* @date
*
* @author
*/

#include <unistd.h>
#include <systick.h>

#define UNUSED __attribute__((unused))

// to use: set reload, enable timer, enable systick interrup

// systick registers
#define SYSTICK_BASE (0xE000E010UL)
#define STK_CTRL (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define STK_LOAD (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define STK_VAL (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))

// masks for ctrl and status reg
#define STK_CTRL_ENABLE (1UL << 0)
#define STK_CTRL_TICKINT (1UL << 1)
#define STK_CTRL_CLKSOURCE (1UL << 2)

#define CPU_HZ (16000000UL) // uhhhh this is what chat said but im gonna recalculate based on rcc registers
#define TICKS_PER_SECOND (1000)
#define SYSTICK_RELOAD ((CPU_HZ/TICKS_PER_SECOND - 1))

// elapsed ticks
static volatile uint32_t elapsed_ticks = 0;

void systick_init() {
    elapsed_ticks = 0;
    STK_CTRL = 0;
    STK_LOAD = SYSTICK_RELOAD;
    STK_VAL = 0; // clear the counter
    STK_CTRL = STK_CTRL_CLKSOURCE | STK_CTRL_TICKINT | STK_CTRL_ENABLE;
}

// block the processor for ticks milliseconds
void systick_delay(uint32_t ticks) {
    uint32_t start = systick_get_ticks();
    while ((systick_get_ticks() - start) < ticks) {
        // block
    }
}

// returns the total elapsed ticks (times the handler was called after boot)
uint32_t systick_get_ticks() {
    return elapsed_ticks;
}

// systick exceptioin handler
void systick_c_handler() {
    elapsed_ticks++;
}
