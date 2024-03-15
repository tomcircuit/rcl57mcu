
#ifndef uni_eeprom_h
#define uni_eeprom_h

#include "stm32f10x.h"

/* macros for MEMIO pin maniuplation */
#define MEMIO_LOW  GPIOB->BRR = GPIO_Pin_0
#define MEMIO_HIGH GPIOB->BSRR = GPIO_Pin_0
#define MEMIO_SET (GPIOB->IDR & GPIO_IDR_IDR0)

/* macros for TIM3 bit timing */
#define TIM3_START TIM3->CR1 |= TIM_CR1_CEN;
#define TIM3_STOP  TIM3->CR1 &= ~(TIM_CR1_CEN)
#define TIM3_UPDATE TIM3->EGR |= TIM_EGR_UG

#define TIM3_BIT (64u)
#define TIM3_HALF_BIT (TIM3_BIT / 2)
#define TIM3_QUARTER_BIT (TIM3_BIT / 4)
#define TIM3_THREE_QUARTER_BIT (TIM3_HALF_BIT + TIM3_QUARTER_BIT)
#define TIM3_DELAY while (TIM3->CR1 & TIM_CR1_CEN) __NOP()
	
/* constants for 11AA080 EEPROM */
#define EEPROM_START_HEADER (0x55)
#define EEPROM_DEVICE_ADDRESS (0xA0)
#define EEPROM_MAK (1)
#define EEPROM_NoMAK (0)
#define EEPROM_SAK (1)
#define EEPROM_NoSAK (0)
#define EEPROM_WREN_CMD (0x96)
#define EEPROM_WRITE_CMD (0x6C)
#define EEPROM_RDSR_CMD (0x05)
#define EEPROM_READ_CMD (0x03)


/** STM32F103 TIM3 Initialization */
void InitTIM3(void);

/** TIM3-based 500ns/tick delay function */
void Delay500ns(uint16_t ticks);

/** Init TIM3 and place EEPROM in standby mode */
void eeprom_standby(void);

/** issue EEPROM read command, return SAK status */
int eeprom_read_command(int addr);

/* Send a byte to the EEPROM and return SAK status */
int eeprom_send_byte(unsigned char d);

/** Function to obtain a 16-bit hash for program sequences.
 * This is used mostly to serve as a program "label". */
/* see: https://en.wikipedia.org/wiki/Fletcher%27s_checksum */
unsigned int fletcher16(unsigned int* data, int count);

#endif /* uni_eeprom_h */