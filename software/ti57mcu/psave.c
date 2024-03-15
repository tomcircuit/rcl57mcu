#include "stm32f10x.h"
#include "ti57.h"
#include "mux57.h"
#include "rcl57mcu.h"
#include "psave.h"

/////////////

/* power save mode function */
void mode_powersave(void)
{
    unsigned char scancode = 0;

    /* make sure TLC5929 is in PowerSave mode */
    hw_digit_driver_load(0);

    while (scancode == 0)
    {
        /* set SysTick to a much slower interval to conserve battery power */
        SysTick_Config(SystemCoreClock / PSAVE_IDLE_SYSTICK_TIMER_FREQ);

        /* turn OFF the D12 direct output */
        GPIOB->BSRR = GPIO_Pin_4;

        /* enable all segment drivers to scan all keyboard rows */
        hw_segment_enable_all();

        /* delay in standby mode for PSAVE_IDLE_TICKS * IDLE_SYSTICK_PERIOD_US. Read keys between timeouts */
        for (long u = PSAVE_IDLE_TICKS; u > 0; u--)
        {
            if (scancode == 0)
            {
                __WFI();
                scancode = (GPIO_ReadInputData(GPIOA) & 0x001f);

								// DEBUG instruction duration tick toggle
								GPIOC->BSRR = GPIO_Pin_14;
								GPIOC->BRR = GPIO_Pin_14;							
            }
        }

        // DEBUG turn on the BluePill LED
        GPIOC->BRR = GPIO_Pin_13;

        /* set SysTick back to normal interval during display time */
        SysTick_Config(SystemCoreClock / SYSTICK_TIMER_FREQ);
				
        /* blink the decimal point at digit 12 while in powersave mode */
        for (int u = PSAVE_BLINK_TICKS; u > 0; u--)
        {
            if (scancode == 0)
                /* do a 6.4ms display cycle using only D12 */
                scancode = hw_display_char_d12(CHAR_CODE_POINT);
        }
        // DEBUG turn off the BluePill LED
        GPIOC->BSRR = GPIO_Pin_13;
    }
}