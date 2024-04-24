#ifndef addon57_h
#define addon57_h

#include "rcl57mcu.h"
#include <stdbool.h>

typedef unsigned char shadow_status_t[24];

#define STAT_SLOT_VACANT (0)
#define STAT_SLOT_OCCUPIED (1)
#define STAT_SLOT_LOCKED (128+1)

#define EE_VALID_SENTINEL (0x57)
#define EE_OFFSET_VALID (0)
#define EE_OFFSET_STATUS_S0 (1)
#define EE_OFFSET_BRIGHT (21)
#define EE_OFFSET_22 (22)
#define EE_OFFSET_23 (23)
#define EE_OFFSET_STEPS_S0 (24)
#define EE_STEPS_PER_SLOT (50)

/** validate EEPROM status block */
bool addon57_validate_status_block(shadow_status_t* stat);

/** populate EEPROM hash block */
//bool addon57_populate_hash_block(shadow_status_t* stat, hash_block_t* hash);

/** PowerSave mode function */
void addon57_powersave(void);

/* following constants configure the "Power Save" mode, which
 * is entered when the keyboard is left idle during many successive
 * DISP instructions (configurable) */

/* Power Save mode defaults:
 94000 IDLE DISP CYCLES --> about 10 minutes
 10000us Systick period during IDLE --> 10ms
 5000ms IDLE period --> 5s
 250ms BLINK period (flash the digit 12 DP)
*/

#define PSAVE_ENTRY_IDLE_DISP_CYCLES (94000)
#define PSAVE_IDLE_SYSTICK_PERIOD_US (10000)
#define PSAVE_IDLE_SYSTICK_TIMER_FREQ (1000000 / PSAVE_IDLE_SYSTICK_PERIOD_US)
#define PSAVE_IDLE_MS (5000)
#define PSAVE_IDLE_TICKS (PSAVE_IDLE_MS * 1000 / PSAVE_IDLE_SYSTICK_PERIOD_US)
#define PSAVE_BLINK_MS (250)
#define PSAVE_BLINK_TICKS (PSAVE_BLINK_MS * 1000 / DISPLAY_PERIOD_US)

#endif /* addon57_h */



