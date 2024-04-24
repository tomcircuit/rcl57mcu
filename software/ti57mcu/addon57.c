#include "stm32f10x.h"
#include "rcl57mcu.h"
#include "addon57.h"
#include "mux57.h"
#include "unio.h"
#include "usart_utilities.h"
#include <stdbool.h>


/* "add-on" functionality for rcl57mcu */

/* These are capabilities that are unique to the rcl57mcu environment.

    So far, these include:

    . power-save mode -- after 10 minutes, the display blanks to save battery
                         D12 decimal point will flash briefly to indicate PS mode
                         pressing any key will return to previous function

    . program-manager -- 20 non-volatile "user program slots" are available for
                         save/recall of programs. Up to 79 "fixed slots" are also
                         available to recall from (stored in flash at compile time)

/*

/////////////

/* read and validate the "status block" from EEPROM. If the contents are
 not valid, the EEPROM is erased and initialized with a default status block.
 This function returns false if there are any issues accessing the EEPROM */
bool addon57_validate_status_block(shadow_status_t* stat)
{
    bool rcode;

    /* Make SRAM shadow does not contain 0x57 */
    (*stat)[EE_OFFSET_VALID] = 0;

    /* Init the UNI/O driver */
    UNIO_init();

    /* Retrieve status block from EEPROM and shadow in SRAM. */
    rcode = UNIO_read(UNIO_EEPROM_ADDRESS, stat, 0, 24);
    if (rcode)
        UU_PutString(USART1, ":R-stat OK");
    else
    {
        UU_PutString(USART1, ":R-stat NOK:");
        (*stat)[EE_OFFSET_VALID] = 0;       // no valid EEPROM found
        (*stat)[EE_OFFSET_BRIGHT] = 15;    // maximum brightness
        return rcode;
    }

    /* Check offset 0 to see if block is valid, if not, init status block */
    if ((*stat)[EE_OFFSET_VALID] == EE_VALID_SENTINEL)
    {
        UU_PutString(USART1, ":Valid");
        //for (int i = 0; i < 24; i++)
        //    UU_PutHexByte(USART1, (*stat)[i]);
    }
    else
    {
        /* erase the EEPROM - first enable writes */
        rcode = UNIO_enable_write(UNIO_EEPROM_ADDRESS);

        /* if write enable passes, do the erase and wait for it to finish */
        if (rcode)
        {
            UU_PutString(USART1, ":Erasing");
            UNIO_erase_all(UNIO_EEPROM_ADDRESS);    // clear all locations to zero
            UNIO_await_write_complete(UNIO_EEPROM_ADDRESS);
            UU_PutString(USART1, ":Complete");
        }
        else
            /* write enable fails, so return with error code and invalid block */
        {
            UU_PutString(USART1, ":Erase NOK:");
            (*stat)[EE_OFFSET_VALID] = 0;       // no valid EEPROM found
            (*stat)[EE_OFFSET_BRIGHT] = 15;    // maximum brightness
            return rcode;
        }

        /* after successful erase, initialize the status block contents */
        (*stat)[EE_OFFSET_VALID] = 0x57;   // mark block as initialized
        for (int i = 0; i < 20; i++)
            (*stat)[EE_OFFSET_STATUS_S0 + i] = STAT_SLOT_VACANT;    // mark slots 0-19 as "vacant"

        (*stat)[EE_OFFSET_BRIGHT] = 15;   // maximum brightness
        (*stat)[EE_OFFSET_22] = 0;          // unused
        (*stat)[EE_OFFSET_23] = 0;          // unused

        /* attempt to write status block to EEPROM */
        rcode = UNIO_simple_write(UNIO_EEPROM_ADDRESS, stat, 0, 24);
        if (rcode)
        {
            UU_PutString(USART1, ":W-stat OK:");
        }
        /* if write fails, return with error code and invalid block */
        else
        {
            UU_PutString(USART1, ":W-stat NOK:");
            (*stat)[EE_OFFSET_VALID] = 0;       // no valid EEPROM found
            (*stat)[EE_OFFSET_BRIGHT] = 15;    // maximum brightness
            return rcode;
        }
    }
    return rcode;
}

/* Function to obtain a 16-bit hash for program sequences.
   This is used mostly to serve as a program "label".
   see: https://en.wikipedia.org/wiki/Fletcher%27s_checksum */

unsigned int fletcher16(unsigned char* data, int count)
{
    unsigned int sum1 = 0;
    unsigned int sum2 = 0;
    int index;

    for (index = 0; index < count; ++index)
    {
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }

    return (sum2 << 8) | sum1;
}

/* calculate the hash for a user program slot from EEPROM */
/* returns false if an EEPROM error has occurred, otherwise true */
bool addon57_calculate_user_slot_hash(unsigned char s, unsigned int* hash)
{
    unsigned char buff[60];
    int i;
    bool rcode;

    /* trim input index to 0-19 */
    i = (s > 19) ? 19 : i;

    /* calculate the starting address in EEPROM of the steps for slot */
    i = i * EE_STEPS_PER_SLOT + EE_OFFSET_STEPS_S0;

    /* retrieve slot steps from EEPROM and shadow in buff array */
    rcode = UNIO_read(UNIO_EEPROM_ADDRESS, buff, i, 50);
    if (rcode)
        UU_PutString(USART1, ":R-slot OK");
    else
        UU_PutString(USART1, ":R-slot NOK:");

    /* so compute hash over buffer contents and save to hash */
    *hash = fletcher16(buff, 50);

    return rcode;
}



/////////////

/* power save mode function */

/* This is entered after PSAVE_ENTRY_IDLE_DISP_CYCLES instances of
   DISPlay instructions have been processed with no keyboard input.
   This function adjusts the SYSTIC timer to a longer interval,
   PSAVE_IDLE_SYSTICK_PERIOD_US, to reduce the number of MCU wakeups.
   During this idle period, all rows of the keypad are sampled each
   SYSTIC interval. If any keys are activated, the idle interval is
   terminated immediately. After PSAVE_IDLE_MS has elapsed, or a key
   was detected, the SYSTIC timer interval is adjusted back to the
   normal interval, and the decimal point of digit #12 is illuminated
   for PSAVE_BLINK_MS. If no key was detected during the idle period,
   the idle-blink intervals repeat. If a key was detected, a brief
   "debounce" is executed to be sure that all keys are released, then
   control is returned back to the calling program. */
void addon57_powersave(void)
{
    unsigned char scancode = 0;
    int count;

    /* make sure TLC5929 is in PowerSave mode by setting all outputs off */
    hw_digit_driver_load(0);
    hw_digit_driver_update();

    while (scancode == 0)
    {
        /* set SysTick back to normal (short) interval during display time */
        SysTick_Config(SystemCoreClock / SYSTICK_TIMER_FREQ);

        /* blink the decimal point at digit 12 while in powersave mode */
        for (int u = PSAVE_BLINK_TICKS; u > 0; u--)
        {
            scancode = hw_display_char_d12(CHAR_CODE_POINT);
        }

        /* set SysTick to a much slower interval to conserve battery power */
        SysTick_Config(SystemCoreClock / PSAVE_IDLE_SYSTICK_TIMER_FREQ);

        /* turn OFF the D12 direct output */
        DIRECT_D12_OFF;

        /* enable all segment drivers to scan all keyboard rows */
        hw_segment_enable_all();

        /* delay in standby mode between blinks - read all keys between timeouts */
        for (long u = PSAVE_IDLE_TICKS; u > 0; u--)
        {
            __WFI();
            DEBUG_TICK_ON;
            DEBUG_TICK_OFF;
            if (scancode != 0)
                break;
            scancode = hw_read_keyboard_row(0);
        }
    }

    /* set SysTick back to normal interval during display time */
    SysTick_Config(SystemCoreClock / SYSTICK_TIMER_FREQ);

    /* turn OFF the D12 direct output */
    DIRECT_D12_OFF;

    /* enable all segment drivers to scan all keyboard rows */
    hw_segment_enable_all();

    /* wait for release of keypress before returning back to TI-57 engine */
    DEBUG_TICK_ON;
    hw_wait_for_key_release(30);        //30*10ms debounce period
    DEBUG_TICK_OFF;
}