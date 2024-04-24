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

#include "mux57.h"
#include "stm32f10x.h"
#include "rcl57mcu.h"
#include "usart_utilities.h"

/* Multiplex LED and keyboard support for RCL-57 retrofit PCB V2 */
/* https://hackaday.io/project/194963 */

/* Private functions */

/* send a 17-bit serial word to TLC5929; bit 17 is in r,  bits 16-1 are in d */
void hw_digit_driver_send_word(uint8_t r, uint16_t d);

/* raw keyboard K1-K5 data for each row */
static uint8_t raw_keyboard_inputs[8];

/* determine key scancode from K1-K5 inputs and specified segment */
uint8_t hw_read_keyboard_row(uint8_t seg);

/* determine key scancode from K1-K5 ADC results and specified segment */
uint8_t hw_read_keyboard_adc_row(uint8_t seg);

/* return the "raw" K1-K5 key column inputs obtained during hw_display_cycle */
uint8_t hw_raw_keyboard_row(uint8_t s);

/* Private data */

/* map of segments required for each of the LED characters */

#define SEGMENT_A (5)
#define SEGMENT_B (2)
#define SEGMENT_C (4)
#define SEGMENT_D (6)
#define SEGMENT_E (0)
#define SEGMENT_F (1)
#define SEGMENT_G (3)
#define SEGMENT_P (7)

#define DISPLAY_MASK_SHOW   (0x0)
#define DISPLAY_MASK_BLANK  (0x8)
#define DISPLAY_MASK_MINUS  (0x1)
#define DISPLAY_MASK_POINT  (0x2)


/*  Segment weighting follows TMC1500 (E,F,B,G,C,A,D,P} */
const int8_t SEGMENT_MAP[32][8] =
{
    /*     E  F  B  G  C  A  D  P   */
    /*0*/ {1, 1, 1, 0, 1, 1, 1, 0}, //  0 A-B-C-D-E-F
    /*1*/ {0, 0, 1, 0, 1, 0, 0, 0}, //  1 B-C
    /*2*/ {1, 0, 1, 1, 0, 1, 1, 0}, //  2 A-B-D-E-G
    /*3*/ {0, 0, 1, 1, 1, 1, 1, 0}, //  3 A-B-C-D-G
    /*4*/ {0, 1, 1, 1, 1, 0, 0, 0}, //  4 B-C-F-G
    /*5*/ {0, 1, 0, 1, 1, 1, 1, 0}, //  5 A-C-D-F-G
    /*6*/ {1, 1, 0, 1, 1, 1, 1, 0}, //  6 A-C-D-E-F-G
    /*7*/ {0, 0, 1, 0, 1, 1, 0, 0}, //  7 A-B-C-D-E-F
    /*8*/ {1, 1, 1, 1, 1, 1, 1, 0}, //  8 A-B-C-D-E-F-G
    /*9*/ {0, 1, 1, 1, 1, 1, 1, 0}, //  9 A-B-C-D-F-G
    /*A*/ {1, 1, 1, 1, 1, 1, 0, 0}, // 10 A-B-C-E-F-G
    /*b*/ {1, 1, 0, 1, 1, 0, 1, 0}, // 11 C-D-E-F-G
    /*C*/ {1, 1, 0, 0, 0, 1, 1, 0}, // 12 A-D-E-F
    /*d*/ {1, 0, 1, 1, 1, 0, 1, 0}, // 13 B-C-D-E-G
    /*E*/ {1, 1, 0, 1, 0, 1, 1, 0}, // 14 A-D-E-F-G
    /*F*/ {1, 1, 0, 1, 0, 1, 0, 0}, // 15 A-E-F-G
    /*     E  F  B  G  C  A  D  P   */
    /*G*/ {1, 1, 0, 0, 1, 1, 1, 0}, // 16 A-C-D-E-F
    /*H*/ {1, 1, 1, 1, 1, 0, 0, 0}, // 17 B-C-E-F-G
    /*J*/ {1, 0, 1, 0, 1, 0, 1, 0}, // 18 B-C-D-E
    /*L*/ {1, 1, 0, 0, 0, 0, 1, 0}, // 19 D-E-F
    /*n*/ {1, 0, 0, 1, 1, 0, 0, 0}, // 20 C-E-G
    /*o*/ {1, 0, 0, 1, 1, 0, 1, 0}, // 21 B-C-D-F
    /*P*/ {1, 1, 1, 1, 0, 1, 0, 0}, // 22 A-B-E-F-G
    /*r*/ {1, 0, 0, 1, 0, 0, 0, 0}, // 23 E-G
    /*t*/ {1, 1, 0, 1, 0, 0, 1, 0}, // 24 D-E-F-G
    /*U*/ {1, 1, 1, 0, 1, 0, 1, 0}, // 25 B-C-D-E-F-G
    /*Y*/ {0, 1, 1, 1, 1, 0, 1, 0}, // 26 B-C-D-F-G
    /*"*/ {0, 1, 1, 0, 0, 0, 0, 0}, // 27 B-F
    /*]*/ {0, 0, 1, 0, 1, 1, 1, 0}, // 28 A-B-C-D
    /*.*/ {0, 0, 0, 0, 0, 0, 0, 1}, // 29 DP
    /*-*/ {0, 0, 0, 1, 0, 0, 0, 0}, // 30 G
    /* */ {0, 0, 0, 0, 0, 0, 0, 0}  // 31 <none>
    /*     E  F  B  G  C  A  D  P   */
};


////////////////////////

/* attempt to convert ASCII chracter into a display character code */
uint8_t mux57_char_code(uint8_t c)
{
    uint8_t code;
    switch (toupper(c))
    {
    case '0':    case '1':    case '2':    case '3':    case '4':
    case '5':    case '6':    case '7':    case '8':    case '9':
        code = c - '0'; break;
    case 'A':    case 'B':    case 'C':    case 'D':    case 'E':
    case 'F':    case 'G':    case 'H':
        code = c - 'A' + 10; break;
    case 'J':
        code = 18; break;
    case 'L':
        code = 19; break;
    case 'N':    case 'O':    case 'P':
        code = c - 'N' + 20; break;
    case 'R':
        code = 23; break;
    case 'T':    case 'U':
        code = c - 'T' + 24; break;
    case 'Y':
        code = 26; break;
    case '"':
        code = 27; break;
    case ']':
        code = 28; break;
    case '.':
        code = 29; break;
    case '-':
        code = 30; break;
    default:
        code = CHAR_CODE_BLANK;
    }
    return code;
}

/* convert null-terminated string to arrays of display digit and mask codes */
void mux57_paint_digits(const uint8_t* ins, display_data_t* digits, display_data_t* mask)
{
    int32_t i, j, len;

    /* determine length of input string, up to 12 characters */
    len = 0;
    while ((ins[len] != '\0') && (len < 12))
    {
        len++;
    }

    /* translate input string right-to-left starting from last character */
    i = 0;  // initialize digit index to zero (rightmost digit)
    for (j = len; j > 0; --j)
    {
        (*mask)[i] = DISPLAY_MASK_SHOW;    // make digit visible
        (*digits)[i++] = mux57_char_code(ins[j - 1]); // converted digit code
    }

    /* mask leftmost digits, if needed, to make 12 digits total */
    while (i < 12)
    {
        (*mask)[i++] = DISPLAY_MASK_BLANK;       // do not display this digit
    }
}

/* convert array of digit and mask codes to null-terminated string */
uint8_t* mux57_display_to_str(display_data_t* digits, display_data_t* mask)
{
    const uint8_t DIGITS[] = "0123456789AbCdEFGHJLnoPrtUY'].- ";
    static uint8_t str[25];
    int16_t k = 0;

    // Go through the 12 digits.
    for (uint8_t i = 11; i >= 0; i--)
    {
        // Compute the actual character based on the digit and the mask information
        int8_t c;
        if ((*mask)[i] & 0x8)
        {
            c = ' ';
        }
        else if ((*mask)[i] & 0x1)
        {
            c = '-';
        }
        else
        {
            c = DIGITS[(*digits)[i]];
        }

        // Add character to the string.
        str[k++] = c;

        // Add the decimal point if necessary.
        if ((*mask)[i] & 0x2)
        {
            str[k++] = '.';
        }
    }
    str[k] = 0;

    return str;
}


/* return 0/1 if the specified segment s is to be illuminated in character code c */
uint8_t mux57_is_segment(uint8_t c, uint8_t s)
{
    if (c > CHAR_CODE_BLANK)     // if not a defined character, consider it blank
        c = CHAR_CODE_BLANK;
    return SEGMENT_MAP[c][s & 0x7];
}

/* parse dA (digits) and dB (mask) to determine which digits have segment s illuminated
 *
 * The return value contains '1' in the bit positions corresponding to the
 * digits (e.g. digit 1 is bit 0, digit 2 is bit 1, etc) that must be driven.
 *
 * For example:
 *   ti57->dA = 0000000000123000
 *   ti57->dB = 9999999999000999
 *   s = 0 (segment a)
 *
 *   return value is 0b0000000000011000 --> digits 4 and 5 have segment 'a' active
 *
 *                         111000000000
 *                         210987654321 --> digit # (there are only 12 digits!)
 *
 * This really isn't useful to rcl57 mcu, merely an example of how to parse the dA and
 * dB registers */
uint16_t mux57_which_digits(display_data_t* digits, display_data_t* mask, uint8_t s)
{
    uint16_t d = 0;

    // when segment requested is a-g
    if (s < 7)
    {
        // Go through the 12 digits in descending order
        for (uint8_t i = 11; i >= 0; i--)
        {
            char c;
            // Make room for new bit in lsb of d
            d = d << 1;
            // Assume the character is only based on the digit information
            c = ((*digits)[i] & 0xf); /*0-f*/
            // Replace the character if necessary based on the mask information
            if ((*mask)[i] & DISPLAY_MASK_BLANK)
            {
                c = CHAR_CODE_BLANK; /* blank */
            }
            else if ((*mask)[i] & DISPLAY_MASK_MINUS)
            {
                c = CHAR_CODE_MINUS; /* minus sign */
            }
            // OR in this digit active status (0 or 1)
            d = d | SEGMENT_MAP[c][s];
        }
    }
    // when segment requested is p (decimal point)
    else if (s == SEGMENT_P)
    {
        // Go through the 12 digits in descending order
        for (int i = 11; i >= 0; i--)
        {
            // Shift the exsting d value left one bit
            d = d << 1;
            // indicate if this digit has decimal point active
            if ((*mask)[i] & DISPLAY_MASK_POINT)
                d = d | 1;
        }
    }
    return d;
}
/* parse dA (digits) and dB (mask) contents to determine TLC5929 output word for segment s
 *
 * The return value contains '1' in the bit positions corresponding to the
 * outputs (e.g. LED1 is bit 11, LED2 is bit 10, etc) that must be driven.
 *
 * TLC5929 Output    :  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
 * LED Display Digit : 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1
 *
 * TLC5929 outputs 12-15 are not used and are typically left off (0 in output register)
 *
 * Had I the space for more components on the PCB, I would have used one or more
 * of the unused outputs as a means to dump the charge on the PMOS segment drivers
 * at the end of the display cycle. This would take at least two dual diode packages
 * to implement, however. For anyone making their own circuit around this design,
 * I do recommend including this - it would allow simplyfing the hw_display_update()
 * function significantly.
 *
 */
uint16_t mux57_which_outputs(display_data_t* digits, display_data_t* mask, uint8_t s)
{
    uint16_t d = 0;

    // when segment requested is a-g
    if (s != SEGMENT_P)
    {
        // Go through the 12 digits in ascending order (OUT0 = d12)
        for (uint8_t i = 0; i < 12; i++)
        {
            char c;

            // Make room for new bit in lsb of d
            d = d << 1;

            // Assume the character is only based on the digit information
            c = (*digits)[i];
            if (c > CHAR_CODE_BLANK)
                c = CHAR_CODE_BLANK; /* blank */

            // Replace the character if necessary based on the mask information
            if ((*mask)[i] & DISPLAY_MASK_BLANK)
            {
                c = CHAR_CODE_BLANK; /* blank */
            }
            else if ((*mask)[i] & DISPLAY_MASK_MINUS)
            {
                c = CHAR_CODE_MINUS; /* minus sign */
            }
            // if digit is active, set the lsb
            if (SEGMENT_MAP[c][s])
                d |= 1 ;
        }
    }
    // when segment requested is p (decimal point)
    else if (s == SEGMENT_P)
    {
        // Go through the 12 digits in ascending order (OUT0 = d12)
        for (int i = 0; i < 12; i++)
        {
            // Make room for new bit in lsb of d
            d = d << 1;

            // indicate if this digit has decimal point active
            if ((*mask)[i] & DISPLAY_MASK_POINT)
                d |= 1 ;
        }
    }

    return d;
}

/* perform a 6.4ms long display update cycle, using DIGITS and MASK */
/* return value is a scancode (or 0) collected during display scanning */
uint8_t hw_display_cycle(display_data_t* digits, display_data_t* mask)
{
    uint8_t segment = 0;  // segment counter 0-7
    uint16_t d_outputs = 0; // digit driver output data
    uint8_t scancode = 0; // keyboard scancode

    /* preload the TLC5929 SR with segment #0 (SEGMENT E) digit outputs */
    /* determine which digits have segment E illuminated */
    d_outputs = mux57_which_outputs(*digits, *mask, SEGMENT_E);

    /* send the pattern to the TLC5929 digit driver */
    hw_digit_driver_load(d_outputs);

    /* make sure the 12th digit 'direct drive' output is floating */
    DIRECT_D12_OFF;

    /* Wait for SysTick to start segment scan sequence */
    //__WFI();

    /* loop through the 8 segments, 800us per segment */
    for (segment = 0; segment < 8; segment++)
    {
        /* disable all segment drive outputs */
        hw_segment_disable_all();

        /* drive previously loaded digit pattern to TLC5929 outputs */
        hw_digit_driver_update();

        /* handle segment activation depending on if some/no digits are to be active */
        if (d_outputs != 0)
        {
            /* case 1: this segement is active in at least one digit (1's in d_outputs) */
            /* enable the appropriate segment drive output */
            hw_segment_enable(segment);

            /* give time for the LED segment in selected digit(s) to illuminate */
            for (int u = SEGMENT_ACTIVE_TICKS; u > 0; u--)
                __WFI();

            /* read this segment's keyboard scancode. Save it if no scancode already held */
            if (scancode == 0)
                scancode = hw_read_keyboard_adc_row(segment);

            /* turn of all segment outputs (takes a while for PMOS to turn off) */
            hw_segment_disable_all();
        }
        else
        {
            /* case 2: no digits need this segment activated - so only read the keypad row */
            /* enable the appropriate segment drive output */
            hw_segment_enable(segment);

            /* wait for 2 ticks (100us) with the segment driver active */
            __WFI();
            __WFI();

            /* read this segment's keyboard scancode. Save it if no scancode already held */
            if (scancode == 0)
                scancode = hw_read_keyboard_adc_row(segment);

            /* turn of all segment outputs (takes a while for PMOS to turn off) */
            hw_segment_disable_all();

            /* wait for all but 2 of SEGMENT ACTIVE TICKS with segment deactivated */
            for (int u = SEGMENT_ACTIVE_TICKS - 2; u > 0; u--)
                __WFI();
        }

        /* determine which digit pattern to preload for next segment */
        /* or all off (0's) in the case of all segments completed */
        if (segment < 7)
            d_outputs = mux57_which_outputs(*digits, *mask, segment + 1);
        else
            d_outputs = 0;

        /* load the digit pattern into the TLC5929 for next segment period*/
        hw_digit_driver_load(d_outputs);

        /* wait for remainder of segment interval */
        for (int u = SEGMENT_INACTIVE_TICKS; u > 0; u--)
            __WFI();
    }

    /* disable all segment drive outputs - just in case */
    hw_segment_disable_all();

    /* update the TLC5929 to all outputs off (enter power-save mode) */
    hw_digit_driver_update();

    /* Scancode = 100 if Program Manager should be entered - 2ND + INV + CLR */
    if (hw_raw_keyboard_row(0) == 0x13)
        scancode = 100;

    return scancode;
}

/* display a single character at digit 12 in a 6.4ms cycle */
/* return value is a scancode (or 0) collected during display scanning */
uint8_t hw_display_char_d12(uint8_t c)
{
    uint8_t segment = 0;  // segment counter 0-7
    uint8_t scancode = 0; // keyboard scancode

    /* Wait for SysTick to start segment scan sequence */
    __WFI();

    /* loop through the 8 segments, 800us per segment */
    for (segment = 0; segment < 8; segment++)
    {
        /* disable all segment drive outputs */
        hw_segment_disable_all();

        /* turn off D12 digit driver */
        DIRECT_D12_OFF;

        /* enable the appropriate segment drive output */
        hw_segment_enable(segment);

        /* handle segment activation depending on if some/no digits are to be active */
        /* case 1: digit 12 needs this segement illumintaed - drive for 700us */
        if (mux57_is_segment(c, segment))
        {
            /* turn on DIG12 if segment is needed for this character */
            DIRECT_D12_ON;    // drive D12DIR low

            /* allow the LED segment in selected digit(s) to illuminate */
            for (uint8_t u = SEGMENT_ACTIVE_TICKS; u > 0; u--)
                __WFI();

            /* read this segment's keyboard scancode. Save it if no scancode already held */
            if (scancode == 0)
                scancode = hw_read_keyboard_adc_row(segment);

            /* turn of all segment outputs (takes a while for PMOS to turn off) */
            hw_segment_disable_all();
        }
        else
            /* case 2: segment not illuminuated in digit 12 - drive for 100us only (for key read)*/
        {
            /* wait for 2 ticks (100us) with the segment driver active */
            __WFI();
            __WFI();

            /* read this segment's keyboard scancode. Save it if no scancode already held */
            if (scancode == 0)
                scancode = hw_read_keyboard_adc_row(segment);

            /* turn of all segment outputs (takes a while for PMOS to turn off) */
            hw_segment_disable_all();

            /* wait out the remainder of SEGMENT ACTIVE TICKS with segment deactivated */
            for (uint8_t u = SEGMENT_ACTIVE_TICKS - 2; u > 0; u--)
                __WFI();

        }
    }

    /* disable all segment drive outputs */
    hw_segment_disable_all();

    /* turn D12DIR output back off (floating high) */
    DIRECT_D12_OFF;

    return scancode;
}

/* display a string to LED for n-display cycles */
void mux57_splash(const uint8_t* ins, uint32_t n)
{
    display_data_t codes, masks;

    /* paint the string to the codes and masks arrays */
    mux57_paint_digits(ins, codes, masks);

    for (uint32_t i = n; i > 0; --i)
    {
        DEBUG_TICK_ON;
        hw_display_cycle(codes, masks);
        DEBUG_TICK_OFF;
    }
}


/*************************************/
/* Segment driver specific functions */
/*************************************/

/* disable all segment drive outputs (drive high to turn off PMOS gate)
   !!MCU GPIO assignment dependent - PCB V2 here!! */
void hw_segment_disable_all(void)
{
    GPIOA->BSRR = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
}

/* enable all segment drive outputs (drive low to turn on PMOS gate)
   !!MCU GPIO assignment dependent - PCB V2 here!! */
void hw_segment_enable_all(void)
{
    GPIOA->BRR = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
}

/* enable one specific segment output, 0-7 -->
 *   SEG # --  0   1   2   3   4   5   6   7
 *   LED   --  E   F   B   G   C   A   D   DP
  !! MCU GPIO assignment dependent - PCB V2 !! */
void hw_segment_enable(uint8_t s)
{
    switch (s)
    {
    case 0:
        GPIOA->BRR = GPIO_Pin_9;
        break;      //e
    case 1:
        GPIOA->BRR = GPIO_Pin_10;
        break;      //f
    case 2:
        GPIOA->BRR = GPIO_Pin_7;
        break;      //b
    case 3:
        GPIOA->BRR = GPIO_Pin_11;
        break;      //g
    case 4:
        GPIOA->BRR = GPIO_Pin_8;
        break;      //c
    case 5:
        GPIOA->BRR = GPIO_Pin_6;
        break;      //a
    case 6:
        GPIOA->BRR = GPIO_Pin_5;
        break;      //d
    case 7:
        GPIOA->BRR = GPIO_Pin_12;
        break;      //dp
    }
}

/*******************************************/
/* TLC5929 Digit Driver specific functions */
/*******************************************/

/* reset the TLC5929 digit driver LATCH signal */
void hw_digit_driver_reset(void)
{
    /* Negate LATCH */
    GPIOA->BRR = GPIO_Pin_15;
}

/* Initialize the TLC5929 digit driver IC by turning all outputs off,
 *  setting Global Brightness level to 50%, and enabling Power Save */
void hw_digit_driver_initialize(void)
{
    hw_digit_driver_send_word(0, 0); // turn off ALL digit drive outputs
    hw_digit_driver_update(); // latch in the output data
    hw_digit_driver_send_word(1, TLC5929_CONTROL); // send control word
    hw_digit_driver_update(); // latch in the control word
}

/* Set the display dimming based upon 0-15 value, using a lookup
 * table to attempt to make the brightness steps more linear */
void hw_digit_driver_intensity(uint8_t i)
{
    unsigned const char DIMMING[16] =
    {
        5,    9,  12,  18,  23,  31,  38,  44,
        51,  58,  67,  76,  86,  96, 109, 127
    };

    hw_digit_driver_send_word(1, (1 << 15) + DIMMING[i & 0x0f]);
    hw_digit_driver_update();
}

/* send output data to the TLC5929 */
void hw_digit_driver_send_word(uint8_t r, uint16_t d)
{
    /* Enable the SPI interface */
    SPI_Cmd(SPI1, ENABLE);

    /* send bits 23-16 -- only bit 16 is held by TLC5929 */
    SPI_I2S_SendData(SPI1, r);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));
    /* send bits 15-8 */
    SPI_I2S_SendData(SPI1, (d >> 8) & 0xff);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));
    /* send bits 7-0 */
    SPI_I2S_SendData(SPI1, (d & 0xff));
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));

    /* Disable the SPI interface */
    SPI_Cmd(SPI1, ENABLE);
}

/* Update the TLC5929 driver outputs by toggling the LATCH signal */
void hw_digit_driver_update(void)
{
    /* Assert LATCH */
    GPIOA->BSRR = GPIO_Pin_15;
    /* delay for some uncritical hold time before negating LATCH */
    __NOP();
    __NOP();
    GPIOA->BRR = GPIO_Pin_15;
}

/* Shift a serial word into the TLC5929 output driver register */
void hw_digit_driver_load(uint16_t d)
{
    hw_digit_driver_send_word(0, d);
}

/*******************************/
/* Keyboard specific functions */
/*******************************/

#define COLUMN_ADC_THRESHOLD (3000U)
#define ADC_EOC_TIMEOUT (1000U)
#define ADC_ERROR_CODE (0xFF)

/* encode the K1-K5 key column ADC results and active segment into a scancode */
uint8_t hw_read_keyboard_adc_row(uint8_t seg)
{
    uint8_t code = 0;
    uint8_t kb_row = 0;
    uint8_t kb_col = 0;
    uint16_t k_result;
    uint16_t adc_timeout;

    /* Enable the ADC */
    ADC_Cmd(ADC1, ENABLE);

    /* clear "raw data" in this segment's row */
    raw_keyboard_inputs[seg & 0x7] = 0;

    /* Start ADC "Regular Conversion" on CH0  */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    /* Start ADC "Injected Conversion Sequence" on CH1-4  */
    ADC_SoftwareStartInjectedConvCmd(ADC1, ENABLE);

    /* wait for both normal and injected conversions to complete */
    for (adc_timeout = ADC_EOC_TIMEOUT; adc_timeout > 0; --adc_timeout)
    {
        if ((ADC1->SR & ADC_SR_EOC) && (ADC1->SR & ADC_SR_JEOC))
            break;
    }
    /* if conversion did not complete, return an error code */
    if (adc_timeout == 0)
        return ADC_ERROR_CODE;

    /* Clear the end-of-conversion flags */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    ADC_ClearFlag(ADC1, ADC_FLAG_JEOC);

    /* iterate through results for columns 5-1 */
    for (uint8_t k = 5; k > 0; --k)
    {
        /* shift raw readings left to make room for new lsb */
        raw_keyboard_inputs[seg & 0x7] = raw_keyboard_inputs[seg & 0x7] << 1;

        /* retrieve correct ADC result for a given column */
        switch (k)
        {
        case 1:
            k_result = ADC1->DR;
            break;      //ADC DR holds 'K1'
        case 2:
            k_result = ADC1->JDR1;
            break;      //ADC JDR1 holds 'K2'
        case 3:
            k_result = ADC1->JDR2;
            break;      //ADC JDR2 holds 'K3'
        case 4:
            k_result = ADC1->JDR3;
            break;      //ADC JDR3 holds 'K4'
        case 5:
        default:
            k_result = ADC1->JDR4;
            break;      //ADC JDR1 holds 'K5'
        }

        /* if the ADC results exceeds a threshold, keyswitch in that column is closed */
        if (k_result > COLUMN_ADC_THRESHOLD)
        {
            /* set bit for column in "raw reading" */
            raw_keyboard_inputs[seg & 0x7] |= 1;
            /* indicate column in which keyswitch is closed */
            kb_col = k;
            /* convert segment number (0-7) to row number (1-8) */
            kb_row = (seg & 0x7) + 1;
        }
    }

    /* Disable the ADC */
    ADC_Cmd(ADC1, DISABLE);

    /* code will be 0 (no key) or a key scancode (11..85) or an ADC error (FF) */
    code = ((kb_row << 4) | kb_col);
    return code;
}


/* encode the K1-K5 key column inputs and active segment into a scancode */
uint8_t hw_read_keyboard_row(uint8_t s)
{
    uint8_t code = 0;
    uint8_t kb_row;
    uint8_t kb_col;
    uint16_t k_inputs;

    /* read the K1-K5 switch inputs */
    k_inputs = (GPIO_ReadInputData(GPIOA) & 0x001f);

    /* save to "raw data" array */
    raw_keyboard_inputs[s & 0x7] = k_inputs;

    /* convert segment number (0-7) to row number (1-8) */
    kb_row = (s & 0x7) + 1;

    /* determine column number from k_inputs bits */
    if (k_inputs & 1)
        kb_col = 1;
    else if (k_inputs & 2)
        kb_col = 2;
    else if (k_inputs & 4)
        kb_col = 3;
    else if (k_inputs & 8)
        kb_col = 4;
    else if (k_inputs & 16)
        kb_col = 5;
    else
    {
        kb_col = 0;
        kb_row = 0;
    }

    /* code will be either 0 or a key scancode (11..85) */
    code = ((kb_row << 4) | kb_col);
    return code;
}

/* return the K1-K5 key column inputs obtained through read_keyboard_row() */
uint8_t hw_raw_keyboard_row(uint8_t s)
{
    return raw_keyboard_inputs[s & 0x7];
}

/* wait for a key to be released - assumes segment(s) already enabled */
void hw_wait_for_key_release(uint32_t t)
{
    uint32_t count;
    uint16_t k_inputs;

    /* get an initial read of key inputs */
    k_inputs = (GPIO_ReadInputData(GPIOA) & 0x001f);
    count = t;

    while ((k_inputs != 0) || (count > 0))
    {
        Delay(10000 / 50);
        k_inputs = (GPIO_ReadInputData(GPIOA) & 0x001f);
        if (k_inputs == 0)
            count = count - 1;
        else
            count = t;
    }
}