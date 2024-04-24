/* Copyright (C) 2011 by Stephen Early <steve@greenend.org.uk>
Copyright (C) 2017 by Jeroen Lanting <https://github.com/NESFreak>
Copyright (C) 2024 by Tom LeMense <https:github.com/tomcircuit>

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

#ifndef unio_h
#define unio_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "stm32f10x.h"

/* Functions to access Microchip UNI/O devices connected to any open-drain
   capable GPIO pin of an STM32F103, such as the 11AA02E48 eeprom chip.
   Multiple UNI/O devices may be connected to this pin, provided they
   have different addresses. The 11AA161 2048-uint8_t EEPROM has address
   0xa1, and so may be connected along with the 11AA02E48 if the
   application needs extra non-volatile storage space. */
#define UNIO_STARTHEADER (0x55)

#define UNIO_EEPROM_ADDRESS (0xa0)
#define UNIO_EEPROM_STATUS_WIP (0x01)

/* Microchip indicate that there may be more device types coming for
   this bus - temperature sensors, display controllers, I/O port
   expanders, A/D converters, and so on.  They will have different
   addresses and so can all be connected to the same pin.  Don't hold
   your breath though! */

/* 11AAxxx EEPROM specific commands */
#define UNIO_EEPROM_READ        0x03
#define UNIO_EEPROM_CRRD        0x06
#define UNIO_EEPROM_WRITE       0x6c
#define UNIO_EEPROM_WREN        0x96
#define UNIO_EEPROM_WRDI        0x91
#define UNIO_EEPROM_RDSR        0x05
#define UNIO_EEPROM_WRSR        0x6e
#define UNIO_EEPROM_ERAL        0x6d
#define UNIO_EEPROM_SETAL       0x67

/* The following are defined in the datasheet as _minimum_ times, in
   microseconds.  There is no maximum. */
#define UNIO_TSTBY_US (600u)
#define UNIO_TSS_US    (10u)
#define UNIO_THDR_US    (5u)

/* Additional margin to all the times defined above to ensure compliance (e.g. fast HSI) */
#define UNIO_MARGIN_US (5u)

/* macros for UNIO pin maniuplation - here, B0 is assumed */
#define UNIO_LOW  do { GPIOB->BRR = GPIO_Pin_0; } while (0)
#define UNIO_HIGH do { GPIOB->BSRR = GPIO_Pin_0; } while (0)
#define UNIO_INP (GPIOB->IDR & GPIO_IDR_IDR0)

/* macros for GPT bit timing */
#define GPT_START do { TIM3->CR1 |= TIM_CR1_CEN; } while (0)
#define GPT_STOP  do { TIM3->CR1 &= ~(TIM_CR1_CEN); } while (0)
#define GPT_RELOAD(val) do { TIM3->ARR = (val); } while (0)
#define GPT_UPDATE do { TIM3->EGR |= TIM_EGR_UG ; } while (0)
#define GPT_DELAY do { __NOP(); } while (TIM3->CR1 & TIM_CR1_CEN)
#define GPT_RUNNING (TIM3->CR1 & TIM_CR1_CEN)

/* UNIO bus timing constants - all derived from UNIO_BIT_US */
#define UNIO_BIT_US (32u)
#define UNIO_HALF_BIT_US (UNIO_BIT_US / 2u)
#define UNIO_QUARTER_BIT_US (UNIO_BIT_US / 4u)
#define UNIO_THREE_QUARTER_BIT_US (UNIO_HALF_BIT_US + UNIO_QUARTER_BIT_US)

/* Put the UNIO bus to sleep. This assumes that both TIM3 and GPIOx
   have already been enabled and initialized */
void UNIO_init();

/* All the following calls return true for success and false for
   failure. */

/* Read from memory into the buffer, starting at 'address' in the
   device, for 'length' uint8_ts.  Note that on failure the buffer may
   still have been overwritten. */
bool UNIO_read(uint8_t unio_address, uint8_t* buffer, uint16_t address, uint16_t length);

/* Write data to memory.  The write must not overlap a page
   boundary; pages are 16 uint8_ts long, starting at zero.  Will return
   false if this condition is not met, or if there is a problem
   communicating with the device.  If the write enable bit is not
   set, this call will appear to succeed but will do nothing.

   This call returns as soon as the data has been sent to the
   device.  The write proceeds in the background.  You must check
   the status register to find out when the write has completed,
   before setting the write enable bit and writing more data.  Call
   await_write_complete() if you want to block until the write is
   finished. */
bool UNIO_start_write(uint8_t unio_address, const uint8_t* buffer, uint16_t address, uint16_t length);

/* Set the write enable bit.  This must be done before EVERY write;
   the bit is cleared on a successful write. */
bool UNIO_enable_write(uint8_t unio_address);

/* Clear the write enable bit. */
bool UNIO_disable_write(uint8_t unio_address);

/* Read the status register into *status.  The bits in this register are:
   0x01 - write in progress
   0x02 - write enable
   0x04 - block protect 0
   0x08 - block protect 1 */
bool UNIO_read_status(uint8_t unio_address, uint8_t* status);

/* Write to the status register.  Bits are as shown above; only bits
   BP0 and BP1 may be written.  Values that may be written are:
   0x00 - entire device may be written
   0x04 - upper quarter of device is write-protected
   0x08 - upper half of device is write-protected
   0x0c - whole device is write-protected

   The write enable bit must be set before a write to the status
   register will succeed.  The bit will be cleared on a successful
   write.  You must wait for the write in progress bit to be clear
   before continuing (call await_write_complete()).  */
bool UNIO_write_status(uint8_t unio_address, uint8_t status);

/* Wait until there is no write operation in progress. */
bool UNIO_await_write_complete(uint8_t unio_address);

/* Write to the device, dealing properly with setting the write
   enable bit, avoiding writing over page boundaries, and waiting
   for the write to complete.  Note that this function may take a
   long time to complete - approximately 5ms per 16 uint8_ts or part
   thereof.  Will NOT alter the write-protect bits, so will not
   write to write-protected parts of the device - although the
   return code will not indicate that this has failed. */
bool UNIO_simple_write(uint8_t unio_address, const uint8_t* buffer, uint16_t address, uint16_t length);

/* Bulk erase the EERPOM.  Must enable writes prior to issuing this command */
bool UNIO_erase_all(uint8_t dev_address);

/* Bulk set the EERPOM to 0xFF.  Must enable writes prior to issuing this command */
bool UNIO_set_all(uint8_t dev_address);


#endif /* #ifndef unio_h */