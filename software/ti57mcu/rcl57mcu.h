#ifndef rcl57mcu_h
#define rcl57mcu_h

/*
    TMC1500 instruction period is 200us, and the DISPlay
    cycle is 32x longer, or 6.4ms. For this retrofit
    I go for 4x the speed instruction rate of TI-57
    but keep the display cycle duration the same to
    preserve the LED display characteristics and other
    TI-57 ROM timing.
*/

#define SYSTICK_PERIOD_US (50)
#define SYSTICK_TIMER_FREQ (1000000 / SYSTICK_PERIOD_US)
#define DISPLAY_PERIOD_US (6400)
#define SEGMENT_PERIOD_US (DISPLAY_PERIOD_US / 8)
#define SEGMENT_TICKS (SEGMENT_PERIOD_US / SYSTICK_PERIOD_US)

#define SCANCODE_PROGMAN_ENTRY (0x99)

#endif /* rcl57mcu_h */



