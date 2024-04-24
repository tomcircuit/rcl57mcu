#include "stm32f10x.h"
#include "ti57.h"
#include "mux57.h"
#include "rcl57mcu.h"
#include "progman.h"

/////////////

/* program manager mode */

program_t user_progs[20];   // RAM copy of USER programs (1020B)

const program_t flash_progs[2] =
{
    [0].status = SLOT_STATUS_OCCUPIED,
    [0].bytes = {
        1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    },
    [0].hash = 1,

    [1].status = SLOT_STATUS_OCCUPIED,
    [1].bytes = {
        2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    },
    [1].hash = 2
};

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


void load_user_progs(void);
{
    unsigned char slot;         // program slot in decimal

    /* copy up to 20 occupied USER slots from EEPROM to user_progs structure in RAM */
    for (int slot = 1; slot < 21; slot++)
    {
        /* Set EEPROM address to slot-1 multiple of 51 (status + 50B) */
        eeprom_read_command((slot - 1) * 51);
        /* clear slot hash field, and read slot status from EEPROM */
        user_progs[slot].hash = 0;
        user_progs[slot].status = eeprom_read_byte(EEPROM_MAK);
        /* if slot status is not FREE, read next 50 bytes from EEPROM and compute hash */
        if (user_progs[slot].status != SLOT_STATUS_FREE)
        {
            for (int i = 0; i < 50; i++)
                user_progs[slot].bytes[i] = eeprom_read_byte(EEPROM_MAK);
            user_progs[slot].hash = fletcher16(&user_progs[slot].bytes[0], 50);
        }
        eeprom_read_byte(EEPROM_NoMAK);             // final read to terminate read sequence
    }

    /* place EEPROM into standby mode */
    eeprom_standby();
}

/* Place EEPROM in standby mode */
void eeprom_standby(void)
{
    /* MEMIO high for > 600us */
    GPIOB->BSRR = GPIO_Pin_0;
    for (int i = 0; i < 16; i++)
        __WFI();
}

/* issue EEPROM read command, return SAK status */
int eeprom_read_command(int addr)
{
    int sak;

    /* send low pulse for at least THDR duration */
    GPIOB->BRR = GPIO_Pin_0;
    __WFI();

    /* send fixed start header and device address bytes */
    eeprom_send_byte(EEPROM_START_HEADER);
    sak = eeprom_send_byte(EEPROM_DEVICE_ADDRESS);
	
		if (sak)
				sak = eeprom_send_byte(EEPROM_READ_CMD);
		
		if (sak)
				sak = eeprom_send_byte(addr >> 8);

		if (sak)
				sak = eeprom_send_byte(addr & 0xff);
		
    return sak
}

/* Transmit a byte to the EEPROM and return SAK status */
int eeprom_send_byte(unsigned char d)
{
	  /* clock in bits 7-0 based upon value in argument 'd' */
    for (int i = 8; i > 0; i--)
    {
        if (d & 0x80u)
            /* msb of d is 1 --> send manchester 1 */
            GPIOB->BRR = GPIO_Pin_0;
            GPIOB->BSRR = GPIO_Pin_0;
        else
            /* MSB of d is 0 --> send manchester 0*/
            GPIOB->BSRR = GPIO_Pin_0;
						GPIOB->BRR = GPIO_Pin_0;

        /* shift d left by 1 to expose next bit */
        d = d << 1;
    }
		/* MAK --> send manchester 1 */
		GPIOB->BRR = GPIO_Pin_0;
		GPIOB->BSRR = GPIO_Pin_0;	
		
		/* receive SAK from EEPROM */
		// delay 3/4 bit
		sak = GPIO_ReadInputData(GPIOB) & 0x01
	
	
}



void mode_progman(void);
{
    ti57_reg_t dA, dB;        // private dA and dB for display contents
    unsigned char slot;         // program slot in decimal
    unsigned char scancode;       // keyboard scancode

    /* blank the entire display */
    for (int i = 0; i < 12, i++)
    {
        dA[i] = 31; // SPACE character
        dB[i] = 0;  // no masking
    }

    /* set the initial program slot to "01" */
    slot = 1;




    progman_slot_display(prog_slot);  // this places the prog_slot number into display memory








}

/** Function to read a


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