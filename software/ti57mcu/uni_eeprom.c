#include "stm32f10x.h"
#include "uni_eeprom.h"

/*
 * Bit-bang Driver for Microchip UNI/O EEPROM
 *  see: https://ww1.microchip.com/downloads/en/devicedoc/ds-22067d.pdf
 *
 * Assumes:  MEMIO connected to GPIO B0 with external pullup resistor
 *           Nominal clock frequency of 30KHz
 *           TIM3 is used for accurate bit timing
 */

/////////////

/* STM32F103 TIM3 Initialization */
void InitTIM3()
{
    TIM_TimeBaseInitTypeDef TimeBaseInitStructure;

    /* Enable the TIM3 clock.   */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* De-init TIM3 to get it back to default values */
    TIM_DeInit(TIM3);

    /* Init the TimeBaseInitStructure with 500ns/tick values */
    TimeBaseInitStructure.TIM_Prescaler = 18 - 1;
    TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TimeBaseInitStructure.TIM_Period = 10 ;
    TIM_TimeBaseInit(TIM3, &TimeBaseInitStructure);
    TIM_SelectOnePulseMode(TIM3, TIM_OPMode_Single);
}

/* TIM3-based 500ns/tick delay function */
void Delay500ns(uint16_t ticks)
{
    /* load the reload-register with target tick value */
    TIM3->ARR = ticks;

    /* force update of the PSC and ARR */
    TIM3->EGR |= TIM_EGR_UG;

    /* start the timer */
    TIM3->CR1  |= TIM_CR1_CEN;

    /* wait for timer to expire */
    while (TIM3->CR1 & TIM_CR1_CEN)
        __NOP();
}

/* Init TIM3 and place EEPROM in standby mode */
void eeprom_standby(void)
{
    /* initialize TIM3 general purpose timer */
    InitTIM3();

    /* MEMIO low for > 10us */
    MEMIO_LOW;
    Delay500ns(10 * 2);

    /* MEMIO high for > 600us */
    MEMIO_HIGH;
    Delay500ns(600 * 2);
}

/* issue EEPROM read command, return SAK status */
int eeprom_read_command(int addr)
{
    int sak = 1;

    /* send low pulse for at least THDR duration */
    MEMIO_LOW;
    Delay500ns(50 * 2);

    /* send 11AA080 START HEADER and DEVICE ADDRESS bytes */
    eeprom_send_byte(EEPROM_START_HEADER);      // no SAK expected
    sak = eeprom_send_byte(EEPROM_DEVICE_ADDRESS);      // SAK expected

    /* if EEPROM ack'd, send the READ command */
    if (1)
        sak = eeprom_send_byte(EEPROM_READ_CMD);

    /* if EEPROM ack'd, send the ADDRESS HIGH byte */
    if (1)
        sak = eeprom_send_byte(addr >> 8);

    /* if EEPROM ack'd, send the ADDRESS LOW byte */
    if (1)
        sak = eeprom_send_byte(addr & 0xff);

    return sak;
}

/* Receive a byte from the EEPROM and issue MAK status */
int eeprom_receive_byte(unsigned char mak)
{
    int sak = 0;
    int s1, s2 ;
    int d = 0;

    /* let MEMIO float to recessive (high) level */
    MEMIO_HIGH;

    /* clock in bits 7-0 based upon value in argument 'd' */
    for (int i = 8; i > 0; i--)
    {
        /* shift D to prepare next bit position */
        d = d << 1;

        /* delay 1/4 bit into the MSB from the EEPROM */
        TIM3->ARR = TIM3_QUARTER_BIT;
        TIM3_UPDATE;
        TIM3_START;
        TIM3_DELAY;

        /* sample the MEMIO line to get first half of manchester bit */
        s1 = MEMIO_SET;

        /* start TIM3 for a 1/2-bit interval */
        TIM3->ARR = TIM3_HALF_BIT;
        TIM3_UPDATE;
        TIM3_START;

        /* poll until MEMIO changes state or TIM3 expires */
        while ((MEMIO_SET == s1) && (TIM3->CR1 & TIM_CR1_CEN))
            __NOP();

        /* at this point, ideally, we are at the 1/2 bit point */

        /* OR the MEMIO state into d lsb */
        s2 = MEMIO_SET;
        if (s2)
            d = d | 1;

        /* complete this bit by delaying 1/2 bit time after edge */
        TIM3_STOP;
        TIM3_UPDATE;
        TIM3_START;
        TIM3_DELAY;

        /* if there was no edge during this bit, exit the loop */
        if (s1 == s2)
            break;
    }

    /* send MAK if specified */
    if (mak)
    {
        /* MAK --> send manchester 1 */
        MEMIO_LOW;
        TIM3_START;
        TIM3_DELAY;
        MEMIO_HIGH;
        TIM3_START;
        TIM3_DELAY;
    }
    else
    {
        /* NoMAK --> send manchester 0 */
        MEMIO_HIGH;
        TIM3_START;
        TIM3_DELAY;
        MEMIO_LOW;
        TIM3_START;
        TIM3_DELAY;
    }

    /* receive SAK from EEPROM */
    TIM3->ARR = TIM3_QUARTER_BIT;
    TIM3_UPDATE;
    TIM3_START;
    TIM3_DELAY;
    if (MEMIO_SET == 0)
        sak = EEPROM_SAK;
    else
        sak = EEPROM_NoSAK;
    TIM3->ARR = TIM3_THREE_QUARTER_BIT;
    TIM3_UPDATE;
    TIM3_START;
    TIM3_DELAY;

    return (d & 0xff);
}

/* Transmit a byte to the EEPROM and return SAK status */
int eeprom_send_byte(unsigned char d)
{
    int sak = 0;

    /* load the TIM3 auto-reload-register with half-bit value */
    TIM3->ARR = TIM3_HALF_BIT;
    TIM3_UPDATE;

    /* clock in bits 7-0 based upon value in argument 'd' */
    for (int i = 8; i > 0; i--)
    {
        if (d & 0x80u)
            /* msb of d is 1 --> send manchester 1 */
        {
            MEMIO_LOW;
            TIM3_START;
            TIM3_DELAY;
            MEMIO_HIGH;
            TIM3_START;
            TIM3_DELAY;
        }
        else
            /* MSB of d is 0 --> send manchester 0*/
        {
            MEMIO_HIGH;
            TIM3_START;
            TIM3_DELAY;
            MEMIO_LOW;
            TIM3_START;
            TIM3_DELAY;
        }
        /* shift d left by 1 to expose next bit */
        d = d << 1;
    }
    /* MAK --> send manchester 1 */
    MEMIO_LOW;
    TIM3_START;
    TIM3_DELAY;
    MEMIO_HIGH;
    TIM3_START;
    TIM3_DELAY;

    /* receive SAK from EEPROM */
    TIM3->ARR = TIM3_QUARTER_BIT;
    TIM3_UPDATE;
    TIM3_START;
    TIM3_DELAY;
    if (MEMIO_SET == 0)
        sak = EEPROM_SAK;
    else
        sak = EEPROM_NoSAK;
    TIM3->ARR = TIM3_THREE_QUARTER_BIT;
    TIM3_UPDATE;
    TIM3_START;
    TIM3_DELAY;

    return sak;
}


/** Function to obtain a 16-bit hash for program sequences.
 * This is used mostly to serve as a program "label". */

/* see: https://en.wikipedia.org/wiki/Fletcher%27s_checksum */

unsigned int fletcher16(unsigned int* data, int count)
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