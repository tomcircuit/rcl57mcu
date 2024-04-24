/* Copyright (C) 2024 by Tom LeMense <https:github.com/tomcircuit>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include "stm32f10x.h"
#include "ti57.h"
#include "rcl57mcu.h"
#include "mux57.h"
#include "addon57.h"

/* Target: STM32F103TBU6 at 24 MHz on RCL57 V2 PCB */
/* https://hackaday.io/project/194963 */

/* STM32F103 peripheral initialization prototypes*/
void InitGPIO();
void InitUSART();
void InitSysTick();
void InitTIM3();
void InitSPI();
void InitADC();

/* SysTick interrupt handler & Delay function */
void SysTick_Handler(void);
void Delay(__IO uint32_t nTime);

/* globals */
static __IO uint32_t TimingDelay;   // Delay() function counter
shadow_status_t ee_status_shadow;   // RAM copy of EEPROM status block

/* varoius strings for 'splash' displays and USART messages */
#ifdef TI57_ROM
    const uint8_t str_version[] = " RCL57 v0.1 ";
    const uint8_t str_splash[] =  "   RCL-57   ";
#endif
#ifdef TI55_ROM
    const uint8_t str_version[] = " RCL55 v0.1 ";
    const uint8_t str_splash[] =  "   RCL-55   ";
#endif
const uint8_t str_ee_fail[] = "   EE FAIL  ";

/////////////

/* this function sets SYSCLK = HCLK = APB1 = APB2 = HSI/2 * 6 = 24 MHz */
/* it also sets flash access to 0WS and enables the CPU prefetch buffer */
void SystemCoreClockConfigure(void)
{

    RCC->CR |= ((uint32_t)RCC_CR_HSION);                     // Enable HSI
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);                  // Wait for HSI Ready

    RCC->CFGR = RCC_CFGR_SW_HSI;                             // HSI is system clock
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);  // Wait for HSI used as system clock

    FLASH->ACR  = FLASH_ACR_PRFTBE;                          // Enable Prefetch Buffer
    FLASH->ACR &= ~FLASH_ACR_LATENCY;                                                // Flash 0 wait state
    FLASH->ACR |= FLASH_ACR_LATENCY_0;

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;                         // HCLK = SYSCLK
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;                        // APB1 = HCLK
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;                        // APB2 = HCLK

    RCC->CR &= ~RCC_CR_PLLON;                                // Disable PLL

    //  PLL configuration:  = HSI/2 * 6 = 24 MHz
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (RCC_CFGR_PLLSRC_HSI_Div2 | RCC_CFGR_PLLMULL6);

    RCC->CR |= RCC_CR_PLLON;                                 // Enable PLL
    while ((RCC->CR & RCC_CR_PLLRDY) == 0)
        __NOP();           // Wait till PLL is ready

    RCC->CFGR &= ~RCC_CFGR_SW;                               // Select PLL as system clock source
    RCC->CFGR |=  RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);  // Wait till PLL is system clock src
}

int main()
{
    ti57_t ti57;
    uint8_t scancode = 0; // row/col of keypress; e.g. 2ND = 0x11 or 0x99 for ProgMan entry
    int16_t cycle_cost = 0;               // "cost" of instruction in terms of TMS1500 cycles (1 for all but DISP which is 32)
    uint32_t num_cycles = 0;     // total running cycles counter
    uint32_t idle_disp_cycles = 0;    // cycles elapsed since last keyboard activity

    bool rcode;        // boolean return code
    uint32_t temp_int;

    uint8_t codes[16], masks[16];

    /* initialize MCU clock tree to 24 MHz, 0WS, prefetch on */
    SystemCoreClockConfigure();
    SystemCoreClockUpdate();

    /* initialize MCU peripherals: GPIO, USART, SysTick, TIM3 */
    InitGPIO();
    InitUSART();
    InitSysTick();
    InitTIM3();
    InitSPI();
    InitADC();

    /* disable all segment driver outputs */
    hw_segment_disable_all();

    /* Initialize TCL5929 digit driver - all digits off, set brightness to 50% */
    hw_digit_driver_initialize();

    /* Disable the "Direct D12" output */
    DIRECT_D12_OFF;

    /* output a welcome string on USART */
    UU_PutString(USART1, str_version);

    /* Display a "splash screen" */
    mux57_splash(str_splash, 300);

    /* Load and validate the EEPROM status block */
    if (addon57_validate_status_block(ee_status_shadow) == false)
    {
        /* alert that EEPROM is not found/not accessible */
        mux57_splash(str_ee_fail, 300);
    }

    /* Set TCL5929 digit driver intensity to value from status shadow */
    hw_digit_driver_intensity(ee_status_shadow[EE_OFFSET_BRIGHT]);

    /* initialize TI-57 emulator */
    ti57_init(&ti57);

    while (1)
    {
        // DEBUG instruction duration tick on
        DEBUG_TICK_ON;

        /* execute the next TMC1500 instruction */
        cycle_cost = ti57_next(&ti57);
        num_cycles += cycle_cost;

        // DEBUG instruction duration tick off
        DEBUG_TICK_OFF;

        // DEBUG press the 1/x key after 18500 cycles
        //if (num_cycles > 18500 && num_cycles < 18600)
        //{
        //      ti57.is_key_pressed = true;
        //      ti57.col = 5;
        //      ti57.row = 2;       // this is 1/x which will cause error displey
        //}

        /* check if display action is required */
        if (ti57.display_update == true)
        {
            /* DEBUG dump the display to USART1 - this increases the display cycle duration!! */
            //UU_PutString(USART1, mux57_display_to_str(ti57.dA, ti57.dB));

            /* The TI-57 ROM always issues 2x DISP instructions due to
               original hardware limitations. Here, the first DISP will check
               the flag, which is not set, and then in this display update
               sequence the flag will be set/clear depending on K inputs. The
               second DISP will catch the flag and process it. */
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
                /* DEBUG dump keypress row,col to UART */
                UU_PutChar(USART1, '#');
                UU_PutHexByte(USART1, scancode);

                /* copy row and column info to ti57 structure */
                ti57.row = scancode >> 4;
                ti57.col = scancode & 0x0F;
                ti57.is_key_pressed = true;

                /* clear the idle keyboard counter */
                idle_disp_cycles = 0;
            }
            /* if 2ND+INV+CLR is pressed, enter PROGRAM MANAGER */
            else if (scancode == SCANCODE_PROGMAN_ENTRY)
            {
                idle_disp_cycles = 0;       // clear the idle keyboard counter
                scancode = 0;
                //mode_progman();
            }
            else if (scancode == 0xFF)
                UU_PutString(USART1, ":ADCfault");

            /* has the keyboard been idle too long? if so, go to powersave */
            if (idle_disp_cycles > PSAVE_ENTRY_IDLE_DISP_CYCLES)
            {
                addon57_powersave();
                idle_disp_cycles = 0;       // clear the idle keyboard counter
                scancode = 0;
            }
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

    /* Configure PA0-PA4 as analog inputs */
    /* These are the K1,K2,K3,K4,K5 keypad column input lines */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure PA5-12 as PP outputs*/
    /* These are the D,A,B,C,E,F,G,P segment control outputs --> init to 1 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8
                                  | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
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
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
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
    /* Step 3 - Enable SPI remap to RB3/RB5*/
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
    /* Step 4 - Configure PB3 and PB5 as AF GPIO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* configure PB6 as USART1 TXD and PB7 as output */
    /* These are brought out to debug header on V2 PCB */
    /* configure PB6 as AF GPIO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    /* Remap USART1 to PB6 */
    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);

    /* configure PB7 as GPIO output */
    /* this could be USART1 RXD if needed */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
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

/* STM32F103 TIM3 Initialization */
void InitTIM3()
{
    TIM_TimeBaseInitTypeDef TimeBaseInitStructure;

    /* Enable the TIM3 clock.   */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* De-init TIM3 to get it back to default values */
    TIM_DeInit(TIM3);

    /* Init the TimeBaseInitStructure with 1us/tick values */
    TimeBaseInitStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TimeBaseInitStructure.TIM_Period = 100;
    TIM_TimeBaseInit(TIM3, &TimeBaseInitStructure);
    TIM_SelectOnePulseMode(TIM3, TIM_OPMode_Single);
}

/* Initialize the STM32F103 SPI peripheral */
void InitSPI(void)
{
    SPI_InitTypeDef  SPI_InitStructure;

    /* enable the SPI1 peripheral clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    /* Load SPI1 InitStructure with config options for CAT4016 */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;

    /* 8-bit data word, MODE = 00, MSB first */
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;

    /* no hardware SS handling */
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;

    /* SPI clock configuration APB2 clock / 8 */
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;

    /* initialize and enable the SPI peripheral */
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

void InitADC(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    /* Enable the ADC clock.   */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* Set the ADC clock prescalar to div-2 (12 MHz) */
    RCC_ADCCLKConfig(RCC_PCLK2_Div2);

    /* ADC1 configuration ------------------------------------------------------*/
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* ADC1 regular channel 1 configuration */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_41Cycles5);

    /* Set injected sequencer length */
    ADC_InjectedSequencerLengthConfig(ADC1, 4);

    /* ADC1 injected channel Configuration */
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_41Cycles5);
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_41Cycles5);
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_41Cycles5);
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_4, 4, ADC_SampleTime_41Cycles5);

    /* ADC1 injected external trigger configuration */
    ADC_ExternalTrigInjectedConvConfig(ADC1, ADC_ExternalTrigInjecConv_None);

    /* Enable injected external trigger conversion on ADC1 */
    ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);

    /* Enable ADC1 */
    ADC_Cmd(ADC1, ENABLE);

    /* Enable ADC1 reset calibration register */
    ADC_ResetCalibration(ADC1);
    /* Check the end of ADC1 reset calibration register */
    while (ADC_GetResetCalibrationStatus(ADC1));

    /* Start ADC1 calibration */
    ADC_StartCalibration(ADC1);
    /* Check the end of ADC1 calibration */
    while (ADC_GetCalibrationStatus(ADC1));
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
