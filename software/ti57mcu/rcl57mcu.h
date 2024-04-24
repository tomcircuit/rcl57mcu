#ifndef rcl57mcu_h
#define rcl57mcu_h

#include <stdint.h>

// choose which ROM to use - TI55 or TI57
//#define TI55_ROM
#define TI57_ROM

// uncomment next line if working on an RCL57 PCB vs. e.g. Blue Pill
#define RCL57_PCB

/*  Some macros to activate a debug pin, and the direct digit
    #12 enable line (open drain output) */

#ifdef RCL57_PCB
    #define DEBUG_TICK_ON  do {GPIOB->BSRR = GPIO_Pin_7;} while (0)
    #define DEBUG_TICK_OFF  do {GPIOB->BRR = GPIO_Pin_7;} while (0)
#else
    #define DEBUG_TICK_ON do {GPIOC->BSRR = GPIO_Pin_14;} while (0)
    #define DEBUG_TICK_OFF do {GPIOC->BRR = GPIO_Pin_14;} while (0)
#endif

#define DIRECT_D12_ON  do {GPIOB->BRR = GPIO_Pin_4;} while (0)
#define DIRECT_D12_OFF do {GPIOB->BSRR = GPIO_Pin_4;} while (0)

/*  TMC1500 instruction period is 200us, and the DISPlay
    cycle is 32x longer, or 6.4ms. For this retrofit
    I go for 4x the speed instruction rate of TI-57
    but keep the display cycle duration the same to
    preserve the LED display characteristics and other
    TI-57 ROM timing. */

#define SYSTICK_PERIOD_US (50)
#define SYSTICK_TIMER_FREQ (1000000 / SYSTICK_PERIOD_US)
#define DISPLAY_PERIOD_US (6400)
#define SEGMENT_PERIOD_US (DISPLAY_PERIOD_US / 8)
#define SEGMENT_TICKS (SEGMENT_PERIOD_US / SYSTICK_PERIOD_US)
#define SEGMENT_INACTIVE_TICKS (2)
#define SEGMENT_ACTIVE_TICKS (SEGMENT_TICKS - SEGMENT_INACTIVE_TICKS)
#define SECONDS_TO_USECS (1000000)

#define SCANCODE_PROGMAN_ENTRY (0x99)


/* types used within rcl57mcu */

static volatile uint16_t AnalogResults[5];	// Array of ADC conversion results


#endif /* rcl57mcu_h */



