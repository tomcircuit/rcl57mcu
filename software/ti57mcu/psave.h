#ifndef psave_h
#define psave_h

#include "rcl57mcu.h"

/** PowerSave mode function */
void mode_powersave(void);

/* following constants configure the "Power Save" mode, which
 * is entered when the keyboard is left idle during many successive
 * DISP instructions (configurable) */

/* Power Save mode defaults:
 20000 IDLE DISP CYCLES --> about 2 minutes
 10000us Systick period during IDLE --> 10ms
 5000ms IDLE period --> 5s
 250ms BLINK period (flash the digit 12 DP)
*/

#define PSAVE_ENTRY_IDLE_PERIOD_MS (4000)
#define PSAVE_ENTRY_IDLE_DISP_CYCLES (PSAVE_ENTRY_IDLE_PERIOD_MS * 1000 / DISPLAY_PERIOD_US)
#define PSAVE_IDLE_SYSTICK_PERIOD_US (10000)
#define PSAVE_IDLE_SYSTICK_TIMER_FREQ (1000000 / PSAVE_IDLE_SYSTICK_PERIOD_US)
#define PSAVE_IDLE_MS (5000)
#define PSAVE_IDLE_TICKS (PSAVE_IDLE_MS * 1000 / PSAVE_IDLE_SYSTICK_PERIOD_US)
#define PSAVE_BLINK_MS (250)
#define PSAVE_BLINK_TICKS (PSAVE_BLINK_MS * 1000 / DISPLAY_PERIOD_US)

#endif /* psave_h */



