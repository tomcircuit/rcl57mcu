#include "stm32f10x.h"
#include "ti57.h"
#include "mux57.h"
#include "rcl57mcu.h"
#include "psave.h"
#include "uni_eeprom.h"

/* Target: STM32F103TBU6 on RCL57 V2 PCB */

/* 24 MHz operating frequency - see RTE_Device.h */

/* STM32F103 peripheral initialization prototypes*/
void InitGPIO();
void InitUSART();
void InitSysTick();

/* SysTick interrupt handler & Delay function */
void SysTick_Handler(void);
void Delay(__IO uint32_t nTime);

/* globals */
static __IO uint32_t TimingDelay;


/////////////

int main()
{
    ti57_t ti57;
    unsigned char scancode = 0; // row/col of keypress; e.g. 2ND = 0x11 or 0x99 for ProgMan entry
    short cycle_cost = 0;
    unsigned long num_cycles = 0;
    unsigned long idle_disp_cycles = 0;

    /* initialize MCU peripherals: GPIO, USART, SysTick */
    InitGPIO();
    InitUSART();
    InitSysTick();

    /* output a welcome string on USART */
    UU_PutString(USART1, "RCL57");

    /* disable all segment driver outputs */
    hw_segment_disable_all();

    /* Initialize TCL5929 digit driver */
    hw_digit_driver_initialize();

    /* initialize TI-57 emulator */
    ti57_init(&ti57);

    // DEBUG turn off PC13 LED
    GPIOC->BSRR = GPIO_Pin_13;

    while (1)
    {
        // DEBUG instruction duration tick on
        GPIOC->BSRR = GPIO_Pin_14;

        /* execute the next TMC1500 instruction */
        cycle_cost = ti57_next(&ti57);
        num_cycles += cycle_cost;

        // DEBUG turn off instruction duration tick
        GPIOC->BRR = GPIO_Pin_14;

        // DEBUG press the 1/x key after 850 cycles
        if (num_cycles > 850 && num_cycles < 950)
        {
            ti57.is_key_pressed = true;
            ti57.col = 5;
            ti57.row = 2;       // this is 1/x which will cause error displey
        }

        /* check if display action is required */
        if (ti57.display_update == true)
        {
            /* DEBUG dump the display to USART1 */
            //UU_PutString(USART1, utils57_display_to_str(ti57.dA, ti57.dB));

            /* clear the key pressed flag - this works because
            is_key_pressed is checked inside the TI57 engine,
                 and because the TI-57 ROM always issues 2x DISP
               instructions - the first DISP will check the flag,
               which is not set. The display update sequence (here)
               will set/clear the flag depending on the K inputs.
               The second DISP will check the flag and process it. */
            ti57.is_key_pressed = false;

            /* do the 6.4ms display cycle, collect keyboard scan codes */
            scancode = hw_display_cycle(ti57.dA, ti57.dB);

            /* increment the idle keyboard cycle counter if no keys detected */
            if (scancode == 0)
            {
                idle_disp_cycles += 1;
            }
            /* if any single keys were pressed during this display cycle,
               copy the keypress information into ti57 structure and let
                           the TI-57 emulation take care of what to do */
            else if (scancode < 0x8F)
            {
                ti57.row = scancode >> 4;
                ti57.col = scancode & 0x0F;
                ti57.is_key_pressed = true;
                idle_disp_cycles = 0;       // clear the idle keyboard counter
            }
            /* if 2ND+INV+CLR is pressed, enter PROGRAM MANAGER */
            else if (scancode == SCANCODE_PROGMAN_ENTRY)
            {
                scancode = 0;
                //mode_progman();
            }

            /* has the keyboard been idle too long? if so, go to powersave */
            if (idle_disp_cycles > PSAVE_ENTRY_IDLE_DISP_CYCLES)
            {
                scancode = 0;
                mode_powersave();
            }
        }

        /* DEBUG dump keypress row,col to UART */
        if (ti57.is_key_pressed)
        {
            UU_PutNumber(USART1, ti57.row);
            UU_PutNumber(USART1, ti57.col);
        }

        /* wait for remainder of SysTick interval in SLEEP mode */
        __WFI();
    }
}


/* STM32F103 GPIO Initialization */
void InitGPIO()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* GPIO are all on APB2 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* enable the AFIO to allow remaps as well */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    /* configure C13 and C14 as GPIO output pins */
    /* Blue Pill Debug pins! LED on C13 (0 = lit) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* GPIO Configuration for RCL-57 Emulator PCB V2*/

    /* Configure PA0-PA4 as inputs will pulldown */
    /* These are the K1,K2,K3,K4,K5 keypad column input lines */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure PA5-12 as PP outputs*/
    /* These are the D,A,B,C,E,F,G,P segment control outputs --> init to 1 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8
                                  | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8
                 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);

    /* configure A15 as GPIO output */
    /* This is TLC5925 LATCH control output --> init to 0 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure PB0 as OD GPIO */
    /* This is 11AA080 UNI/O signal input-output with 47K pullup */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);

    /* Configure PB4 as OD GPIO */
    /* This is a 'direct drive' for digit #12 cathode */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_4);

    /* Configure PB3 and PB5 as PP GPIO */
    /* These are TLC5925 SCLK and SDATA outputs, respectively */
    /* Step 1 - disable JTAG interface (uses PB3) */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    /* Step 2 - Release PB3/TRACESWO and PB4/NJTRST from control of the
    debug port so they can be used as GPIO pins. This info was
    well-hidden in an ST forum posting. */
    DBGMCU->CR &= ~DBGMCU_CR_TRACE_IOEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    /* Step 3 - configure PB3 and uPB5 as GPIO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* configure PB6/PB7 as USART1 TXD & RXD */
    /* These are brought out to debug header on V2 PCB */
    /* configure PB6 as AF GPIO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    /* configure PB7 as AF input */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Finally, remap USART1 to PB6 and PB7 */
    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);
}

/* STM32F103 USART1 initialization */
void InitUSART()
{
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    /* 230400 bps, 8N1, no flow control, TX only */
    USART_InitStructure.USART_BaudRate = 230400;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

/* configure the SysTick timer for correct period */
void InitSysTick()
{
    SysTick_Config(SystemCoreClock / SYSTICK_TIMER_FREQ);
}

/* SysTick interrupt handler - for Delay() function */
void SysTick_Handler(void)
{
    if (TimingDelay != 0x00)
    {
        TimingDelay--;
    }

}

/* simple Delay function, in SysTick increments */
void Delay(__IO uint32_t nTime)
{
    TimingDelay = nTime;
    while (TimingDelay != 0);
}

