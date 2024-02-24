#include "stm32f10x.h"
#include "cat4016.h"

/* Target: STM32F103TBU6 */


/////////////

/* negate (bring low) the CAT4016 LATCH signal */
void hw_cat4016_latch_negate(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_15);
}

/* assert (bring high) the CAT4016 LATCH signal */
void hw_cat4016_latch_assert(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_15);
}

/* negate (bring low) the CAT4106 BLANK signal */
void hw_cat4016_blank_negate(void)
{
	GPIO_ResetBits(GPIOD, GPIO_Pin_0);	
}

/* assert (bring high) the CAT4016 BLANK signal */
void hw_cat4016_blank_assert(void)
{
	GPIO_SetBits(GPIOD, GPIO_Pin_0);	
}

/* Initialize the CAT4016 digit driver */
void hw_cat4016_initialize(void)
{
	hw_cat4016_blank_negate(); // negate the blanking output	
	hw_cat4016_latch_negate(); // make sure latch is low
	// CAT4016 requires >800ns delay (t_lsu)
	for(int u=500;u>0;u--);
	SPI_I2S_SendData(SPI1,0) ;
	// wait for transfer to complete
	while ( SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) );
	hw_cat4016_latch_assert(); // latch in new data (all off)
	hw_cat4016_latch_negate(); // drive to outputs
}

/* Initialize the STM32F103 SPI peripheral */
void hw_cat4016_mcu_config(void)
{
	SPI_InitTypeDef  SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;	
	
	/* Enable SPI remap to RB3/RB5*/
  GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);	

	/* Then, configure PB3 and PB5 as AF GPIO */ 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* enable the SPI1 peripheral clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
	
  /* Load SPI1 InitStructure with config options for CAT4016 */
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

  /* initialize and enable the SPI peripheral */
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_Cmd(SPI1, ENABLE);	
}

/* send digit drive data to the CAT4016 via SPI, wait for completion */
void hw_cat4016_send_data_blocking(unsigned short d)
{
	SPI_I2S_SendData(SPI1, d);
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));	
}

/* start a digit drive data transfer to the CAT4016 via SPI */
void hw_cat4016_send_data_nonblocking(unsigned short d)
{
	SPI_I2S_SendData(SPI1, d);
}

void hw_cat4016_update_outputs(int delay)
{
			/* toggle latch to bring CAT4016 shift-reg contents to outputs */
			hw_cat4016_latch_assert();						
			hw_cat4016_latch_negate();
			
			// need a 2us delay to allow for digit outputs to update
			for(int u=delay;u>0;u--);
}
	

