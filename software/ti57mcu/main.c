#include "stm32f10x.h"
#include "ti57.h"
#include "mux57.h"
#include "cat4016.h"

/* Target: STM32F103TBU6 */

/* 
	TMC1500 instruction period is 200us, and the DISPlay
	cycle is 32x longer, or 6.4ms. For this emulation
	I go for 4x the speed instruction rate of TI-57
	but keep the display cycle duration the same to 
	preserve the LED display characteristics and other 
	TI-57 ROM timing. 
*/

/* 36 MHz operating frequency - see RTE_Device.h */
/* 20 KHz SysTick frequency (50us per tick) */
/* 128 SPI clock prescalar - 58us per 16b shift */

#define SYSTICK_PERIOD_US (50)
#define DISPLAY_PERIOD_US (6400)
#define SEGMENT_PERIOD_US (DISPLAY_PERIOD_US / 8)
#define SEGMENT_TICKS (SEGMENT_PERIOD_US / SYSTICK_PERIOD_US)

/* STM32F103 peripheral initialization prototypes*/
void InitGPIO();
void InitUSART();
void InitSysTick();

/* SysTick interrupt handler & Delay function */
void SysTick_Handler(void);
void Delay(__IO uint32_t nTime);

/* TI-57 EMU hardware specific prototypes */
void hw_segment_disable_all(void);
void hw_segment_enable(unsigned char s);

void hw_display_cycle(ti57_reg_t *digits, ti57_reg_t *mask);
unsigned char hw_read_key_columns(void);

/* globals */
static __IO uint32_t TimingDelay;
/* hold key switch column data for each row scanned */
unsigned char key_data[8];
/* mapping from segment output (0-7 to keyboard row-1) */
const unsigned char kb_row[8] = {4, 5, 1, 6, 2, 0, 3, 7};

/////////////

int main()
{
		ti57_t ti57;
		short cycle_cost = 0;
	  unsigned long num_cycles = 0;
	  char hw_pcb_led = 0;

		/* initialize MCU peripherals */
		InitGPIO();
		InitUSART();
    InitSysTick();

		/* output a welcome string on USART */
		UU_PutString(USART1, "RCL57");
		
		/* Initialize CAT4016 digit driver */
		hw_cat4016_initialize();
	
		/* disable all segment driver outputs */
	  hw_segment_disable_all();
	
		/* initialize TI-57 emulator */
		ti57_init(&ti57);
	
    while(1)
    {
			  // DEBUG instruction duration tick on
				GPIOC->BSRR = GPIO_Pin_14;
			
				/* execute the next TMC1500 instruction */
				cycle_cost = ti57_next(&ti57);
				num_cycles += cycle_cost;

				// DEBUG press the 1/x key after 850 cycles
						if (num_cycles > 850 && num_cycles < 950) {
						ti57.is_key_pressed = true;
						ti57.col = 5;
						ti57.row = 2;		// this is 1/x which will cause error displey
					}
			
				// DEBUG turn off instruction duration tick 
			  GPIOC->BRR = GPIO_Pin_14;

				/* DEBUG dump keypress row,col to UART */
				if (ti57.is_key_pressed)
				{
					UU_PutNumber(USART1,ti57.row);
					UU_PutNumber(USART1,ti57.col);						
				}
						
				/* check if display action is required */
			  if (ti57.display_update == true) {			
					/* DEBUG dump the display to USART1 */
					UU_PutString(USART1, utils57_display_to_str(ti57.dA, ti57.dB));					

					/* clear the key pressed flag - this works because
             is_key_pressed is checked inside the TI57 engine, 
						 and because the TI-57 ROM always issues 2x DISP
					   instructions - the first DISP will check the flag,
					   which is not set. The display update sequence (here)
					   will set/clear the flag depending on the K inputs.
					   The second DISP will check the flag and process it. */
					ti57.is_key_pressed = false;

					/* do the 6.4ms display cycle */
					hw_display_cycle(ti57.dA, ti57.dB);	
					
					/* examine key_data[] to see if any keys were pressed during
					   this display cycle */
					for (unsigned int seg=0;seg<8;seg++)
					{
						unsigned int col;
						switch (key_data[seg])
						{
							case 1: col = 1; break;
							case 2: col = 2; break;
							case 4: col = 3; break;
							case 8: col = 4; break;
							case 16: col = 5; break;
							default: col = 0;
						}						
						if (col != 0) 
						{
							ti57.row = kb_row[seg];
							ti57.col = col;
							ti57.is_key_pressed = true;
						}	
					}						
				}
				else
					/* wait for remainder of SysTick interval in SLEEP mode */
					__WFI();
    }
}

/* TI57 EMU Hardware functions */

/**perform a 6.4ms long display cycle */
void hw_display_cycle(ti57_reg_t *digits, ti57_reg_t *mask)
{
		unsigned char segment = 0;  // segment counter 0-7
		unsigned short d_outputs = 0; // digit bitmask 
					
		/* preload the CAT4016 SR with A-segment digit outputs */
		/* determine which digits have segment A illuminated */
		d_outputs = mux57_which_outputs(*digits, *mask, 0);
					
		/* send the pattern to the CAT4016 shift register and wait for completion */
		hw_cat4016_send_data_blocking(d_outputs);
		
		/* Wait for SysTick to start segment scan sequence */
		__WFI();

		/* loop through the 8 segments, 800us per segment */
		for (segment=0; segment<8; segment++) {
						
			/* read the prior segment's keyboard column data before
				 we switch to the next segment - this allows maximum time
				 after driving segment output before reading switch input */
			if (segment>0)
				key_data[segment-1] = hw_read_key_columns();		
			
			/* disable all segment drive outputs */
			hw_segment_disable_all();
			
			/* output previously loaded digit pattern via CAT4016 */
			hw_cat4016_update_outputs(1000);

			/* enable the appropriate segment drive output */
			hw_segment_enable(segment);

			/* at this point in time, one segement is active in all of the
				 digit(s) that need it */

			/* wait for SEGMENT_TICKS - 2 with segment and digit(s) driven
				 during the last two ticks, the next segment's digit pattern
				 will be shifted out (but not yet loaded). This is because
				 CAT4016 has setup requirement between clock and latch that
				 must be obeyed. */
			for(int u=SEGMENT_TICKS-2;u>0;u--)
				__WFI();
			
			/* determine which digit pattern to preload for next segment */
			/* or all off (0's) in the case of all segments completed */
			if (segment<7)
				d_outputs = mux57_which_outputs(*digits, *mask, segment+1);
			else
				d_outputs = 0;
			
			/* shift the NEXT segment's digit pattern into the CAT4016 */
			hw_cat4016_send_data_nonblocking(d_outputs);
			
			/* wait for final two ticks of this segment period while SPI shifts */
			__WFI();
			__WFI();
		}

		/* read final segment's keyboard column data before we disable all
       the segments one last time. */
		key_data[segment-1] = hw_read_key_columns();

		/* disable all segment drive outputs */
		hw_segment_disable_all();

		/* Toggle CAT4016 LATCH to bring in all 0's (final shift) */
		hw_cat4016_update_outputs(1000);
}					

/* disable all segment drive outputs */
void hw_segment_disable_all(void)
{
	/* disable (set) all segment drive outputs */
	GPIO_SetBits(GPIOA, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
	GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2);
}

/* enable one specific segment output, 0-7 --> a-g,p 
   The mapping in here depends upon the MCU GPIO assignments */
void hw_segment_enable(unsigned char s)
{
	switch(s) {
		case 0: GPIO_ResetBits(GPIOB,GPIO_Pin_1); break;
		case 1: GPIO_ResetBits(GPIOB,GPIO_Pin_2); break;
		case 2: GPIO_ResetBits(GPIOA,GPIO_Pin_8); break;
		case 3: GPIO_ResetBits(GPIOB,GPIO_Pin_0); break;
		case 4: GPIO_ResetBits(GPIOA,GPIO_Pin_9); break;
		case 5: GPIO_ResetBits(GPIOA,GPIO_Pin_10); break;
		case 6: GPIO_ResetBits(GPIOA,GPIO_Pin_11); break;							
		case 7: GPIO_ResetBits(GPIOA,GPIO_Pin_12); break;	
	}	
}

/* read the K1-K5 key column inputs */
unsigned char hw_read_key_columns(void)
{
	return (GPIO_ReadInputData(GPIOA) & 0x00f8) >> 3;
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

	/* GPIO Configuration for TI-57 Emulator PCB */
	
	/* Configure PA3-PA7 as inputs will pulldown */
	/* These are the K1,K2,K3,K4,K5 keypad column input lines */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* Configure PA8-12 as PP outputs*/
	/* These are the C,E,F,G,P segment control outputs --> init to 1 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
	
	/* Configure B0-B2 as PP outputs*/
	/* These are the D,A,B segment control outputs --> init to 1 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2);
	
	/* configure A15 as GPIO output */
	/* This is CAT4016 LATCH control output --> init to 0 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* configure D0 as GPIO output */
	/* This is CAT4016 BLANK control output --> init to 1 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);
	GPIO_SetBits(GPIOD, GPIO_Pin_0);
	
	/* configure PB6/PB7 as USART1 TXD & RXD */

	/* configure PB6 as AF GPIO */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* configure PB7 as AF input */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* first, remap USART1 to PB6 and PB7 */
	GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);	
	
	/* configure B3 and B5 as SPI1 SCK and MOSI */
	/* first, disable JTAG interface (uses PB3) */
  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
  /* Release PB3/TRACESWO and PB4/NJTRST from control of the
  debug port so they can be used as GPIO pins. This info was
	well-hidden in an ST forum posting. */
  DBGMCU->CR &= ~DBGMCU_CR_TRACE_IOEN;
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;	
	
	/* then, remap SPI1 to PB3 and PB5 */
  GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);	

	/* Finally, configure PB3 and PB3 as AF GPIO */ 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* STM32F103 SPI Initialization */
void InitSPI()
{
	SPI_InitTypeDef  SPI_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
	
  /* SPI1 configuration */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	
	/* 16-bit data word, MODE = 00, MSB first */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	
	/* no hardware SS handling */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	
	/* SPI clock configuration APB2 clock / 128 */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;

	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_Cmd(SPI1, ENABLE);	
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

/* configure the SysTick timer for 100us period (10KHz) */
void InitSysTick()
{
        SysTick_Config(SystemCoreClock / 20000);
}

void SysTick_Handler(void)
{
        if (TimingDelay != 0x00)
        {
                TimingDelay--;
        }

}

void Delay(__IO uint32_t nTime)
{
         TimingDelay = nTime;
         while(TimingDelay != 0);
}
