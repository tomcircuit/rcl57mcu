/* Copyright (C) 2011 by Stephen Early <steve@greenend.org.uk>
Copyright (C) 2017 by Jeroen Lanting <https://github.com/NESFreak>

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

#include "UNIO.h"

#include "stm32f10x.h"

/* GPT polled microsecond delay function */
void delayMicroseconds(uint16_t us)
{
    /* load the reload-register with target tick value */
    //TIM3->ARR = (us - 1);
    GPT_RELOAD(us - 1);

    /* force update of the PSC and ARR */
    //TIM3->EGR |= TIM_EGR_UG;
    GPT_UPDATE;

    /* start the timer */
    //TIM3->CR1  |= TIM_CR1_CEN;
    GPT_START;

    /* wait for timer to expire */
    while (TIM3->CR1 & TIM_CR1_CEN)
        __NOP();
}

/* GPT polled transition hunt function - returns true if transition found */
bool huntMicroseconds(uint16_t us)
{
    bool state;
    UNIO_HIGH;      // release bus to pullup

    /* load the reload-register with target tick value */
    //TIM3->ARR = (us - 1);
    GPT_RELOAD(us - 1);

    /* force update of the PSC and ARR */
    //TIM3->EGR |= TIM_EGR_UG;
    GPT_UPDATE;

    /* start the timer */
    //TIM3->CR1  |= TIM_CR1_CEN;
    GPT_START;

    /* capture the input state */
    state = !!(UNIO_INP);

    /* wait for timer to expire OR the state to change */
    while (GPT_RUNNING && (state == !!(UNIO_INP)))
        __NOP();

    /* stop the timer */
    //TIM3->CR1  &= ~TIM_CR1_CEN;
    GPT_STOP;

    /* return true if state changed */
    return (state == !(UNIO_INP));
}


static void unio_set_bus(bool state)
{
    if (state)
        UNIO_HIGH;
    else
        UNIO_LOW;
}

static inline bool unio_read_bus(void)
{
    return !!(UNIO_INP);
}

/* If multiple commands are to be issued to a device without a standby
   pulse in between, the bus must be held high for at least UNIO_TSS
   between the end of one command and the start of the next. */
static void unio_inter_command_gap(void)
{
    UNIO_HIGH;
    delayMicroseconds(UNIO_TSS_US + UNIO_MARGIN_US);
}

/* Send a standby pulse on the bus.  After power-on or brown-out
   reset, the device requires a low-to-high transition on the bus at
   the start of the standby pulse.  To be conservative, we take the
   bus low for UNIO_TSS, then high for UNIO_TSTBY. */
static void unio_standby_pulse(void)
{
    UNIO_LOW;
    delayMicroseconds(UNIO_TSS_US + UNIO_MARGIN_US);
    UNIO_HIGH;
    delayMicroseconds(UNIO_TSTBY_US + UNIO_MARGIN_US);
}

/* While bit-banging, all delays are expressed in terms of UNIO_BIT_US.
   We use the similar code path for reading and writing.  During a
   write, we perform dummy reads at 1/4 and 3/4 of the way through
   each bit time.  During a read we perform a dummy write at the start
   and 1/2 way through each bit time. By using a hardware General Purpose
   timer, this code is not NEARLY as critial as it would be if software
   loops were used. */

static bool unio_send_bit(bool w)
{
    bool a, b;
    unio_set_bus(!w);
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    a = unio_read_bus();
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    unio_set_bus(w);
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    b = unio_read_bus();
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    return b && !a;
}

static bool unio_receive_bit(void)
{
    bool a, b;
    unio_set_bus(1);  // release bus to pullup
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    a = unio_read_bus();
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    unio_set_bus(1);  // release bus to pullup
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    b = unio_read_bus();
    delayMicroseconds(UNIO_QUARTER_BIT_US);
    return b && !a;
}
/* Send a single byte terminated by MAK/NoMAK. Return SAK/NoSAK status */
static bool unio_send_byte(uint8_t b, bool mak)
{
    for (int i = 0; i < 8; i++)
    {
        unio_send_bit(b & 0x80);
        b <<= 1;
    }
    unio_send_bit(mak);           // transmite MAK/NoMAK

    return unio_receive_bit();    // return SAK/NoSAK status
}

/* Receive a single byte terminated by MAK/NoMAK. Return SAK/NoSAK status */
static bool unio_receive_byte(uint8_t* b, bool mak)
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++)
    {
        data = (data << 1) | unio_receive_bit();
    }
    *b = data;
    unio_send_bit(mak);           // issue MAK/NoMAK

    return unio_receive_bit();    // return SAK/NoSAK status
}

/* Send multiple byte data on the bus. If end is true, send NoMAK after the last
   byte; otherwise send MAK. */
static bool unio_send_n_bytes(const uint8_t* data, uint16_t length, bool end)
{
    for (uint16_t i = 0; i < length; i++)
    {
        /* Rules for sending MAK: if it's the last byte and end is true,
           send NoMAK.  Otherwise send MAK. */
        if (!unio_send_byte(data[i], !(((i + 1) == length) && end)))
            return false;
    }
    return true;
}

/* Read data from the bus.  After reading 'length' bytes, send NoMAK to
   terminate the command. */
static bool unio_receive_n_bytes(uint8_t* data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        if (!unio_receive_byte(data + i, !((i + 1) == length)))
            return false;
    }
    return true;
}

/* Put the UNIO bus to sleep. This does NOT init the MCU GPT and GPIO
   resources! That must be done elsewhere. */
void UNIO_init()
{
    GPT_STOP;
    GPT_UPDATE;
    unio_standby_pulse();
}

/* Send a start header on the bus.  Hold the bus low for UNIO_THDR,
   then transmit UNIO_STARTHEADER.  There is a time slot for SAK after
   this transmission, but we must ignore the value because no slave
   devices will drive the bus; if there is more than one device
   connected then that could cause bus contention. */
static void unio_start_header(void)
{
    UNIO_LOW;
    //delayMicroseconds(UNIO_THDR_US + UNIO_MARGIN_US);
    delayMicroseconds(70);
    UNIO_HIGH;
    unio_send_byte(UNIO_STARTHEADER, true);
}

bool UNIO_read(uint8_t dev_address, uint8_t* buffer, uint16_t mem_address, uint16_t length)
{
    bool a, b;
    uint8_t cmd[4];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_READ;
    cmd[2] = (uint8_t)(mem_address >> 8);
    cmd[3] = (uint8_t)(mem_address & 0xff);
    unio_inter_command_gap();
    unio_start_header();
    a = unio_send_n_bytes(cmd, 4, false);
    b = unio_receive_n_bytes(buffer, length);
    return a && b;
}

bool UNIO_start_write(uint8_t dev_address, const uint8_t* buffer, uint16_t mem_address, uint16_t length)
{
    bool a, b;
    uint8_t cmd[4];
    if (((mem_address & 0x0f) + length) > 16)
        return false; // would cross page boundary
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_WRITE;
    cmd[2] = (uint8_t)(mem_address >> 8);
    cmd[3] = (uint8_t)(mem_address & 0xff);
    unio_inter_command_gap();
    unio_start_header();
    a = unio_send_n_bytes(cmd, 4, false);
    b = unio_send_n_bytes(buffer, length, true);
    return a && b;
}

bool UNIO_enable_write(uint8_t dev_address)
{
    uint8_t cmd[2];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_WREN;
    unio_inter_command_gap();
    unio_start_header();
    return unio_send_n_bytes(cmd, 2, true);
}

bool UNIO_disable_write(uint8_t dev_address)
{
    uint8_t cmd[2];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_WRDI;
    unio_inter_command_gap();
    unio_start_header();
    return unio_send_n_bytes(cmd, 2, true);
}

bool UNIO_read_status(uint8_t dev_address, uint8_t* status)
{
    bool a, b;
    uint8_t cmd[2];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_RDSR;
    unio_inter_command_gap();
    unio_start_header();
    a = unio_send_n_bytes(cmd, 2, false);
    b = unio_receive_n_bytes(status, 1);
    return a && b;
}

bool UNIO_write_status(uint8_t dev_address, uint8_t status)
{
    uint8_t cmd[3];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_WRSR;
    cmd[2] = status;
    unio_inter_command_gap();
    unio_start_header();
    return unio_send_n_bytes(cmd, 3, true);
}

bool UNIO_erase_all(uint8_t dev_address)
{
    uint8_t cmd[2];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_ERAL;
    unio_inter_command_gap();
    unio_start_header();
    return unio_send_n_bytes(cmd, 2, true);
}

bool UNIO_set_all(uint8_t dev_address)
{
    uint8_t cmd[2];
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_SETAL;
    unio_inter_command_gap();
    unio_start_header();
    return unio_send_n_bytes(cmd, 2, true);
}

bool UNIO_await_write_complete(uint8_t dev_address)
{
    bool a, b;
    uint8_t cmd[2];
    uint8_t status;
    cmd[0] = dev_address;
    cmd[1] = UNIO_EEPROM_RDSR;
    unio_inter_command_gap();

    /* Here we issue RDSR commands back-to-back until the WIP bit in the
       status register is cleared.  Note that this isn't absolutely the
       most efficient way to monitor this bit; after sending the command
       we could read as many uint8_ts as we like as long as we send MAK
       after each uint8_t.  The unio_read() function isn't set up to do
       this, though, and it's not really performance-critical compared
       to the eeprom write time! We re-enable interrupts briefly between
       each command so that any background tasks like updating millis()
       continue to happen.*/
    do
    {
        unio_inter_command_gap();
        unio_start_header();
        a = unio_send_n_bytes(cmd, 2, false);
        b = unio_receive_n_bytes(&status, 1);
    }
    while (a && b && (status & UNIO_EEPROM_STATUS_WIP));

    return true;
}

bool UNIO_simple_write(uint8_t dev_address, const uint8_t* buffer, uint16_t address, uint16_t length)
{
    uint16_t wlen;
    while (length > 0)
    {
        wlen = length;
        if (((address & 0x0f) + wlen) > 16)
        {
            /* Write would cross a page boundary.  Truncate the write to the
               page boundary. */
            wlen = 16 - (address & 0x0f);
        }
        if (!UNIO_enable_write(dev_address))
            return false;
        if (!UNIO_start_write(dev_address, buffer, address, wlen))
            return false;
        if (!UNIO_await_write_complete(dev_address))
            return false;
        buffer += wlen;
        address += wlen;
        length -= wlen;
    }
    return true;
}
