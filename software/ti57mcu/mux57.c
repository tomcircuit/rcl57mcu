#include "mux57.h"
#include "stm32f10x.h"
#include "rcl57mcu.h"

/* Multiplex LED and keyboard support for RCL-57 retrofit PCB V2
 * March 2024, Tom LeMense
 *
 *       --A--
 *      |     |
 *      F     B
 *      |     |
 *       --G--
 *      |     |
 *      E     C
 *      |     |
 *       --D--  [P]
 *
 * SEG # --  0   1   2   3   4   5   6   7
 * LED   --  E   F   B   G   C   A   D   DP
 *
 * digit 1 (rightmost) cannot illuminate DP
 * digit 12 (leftmost) cannot illuminate D
 *
 * KEYBOARD MAPPING
 *
 *            K     K     K     K     K
 *            1     2     3     4     5
 *
 *   SEG E   2ND   INV   LNX   CE    CLR
 *       0    11    12    13    14    15  <--- scancode
 *
 *   SEG F   LRN   X/T   X^2   vX    1/X
 *       1    21    22    23    24    25
 *
 *   SEG B   SST   STO   RCL   SUM   Y^X
 *       2    31    32    33    34    35
 *
 *   SEG G   BST   EE     (     )     /
 *       3    41    42    43    44    45
 *
 *   SEG C   GTO    7     8     9     X
 *       4    51    52    53    54    55
 *
 *   SEG A   SBR    4     5     6     -
 *       5    61    62    63    64    65
 *
 *   SEG D   RST    1     2     3     +
 *       6    71    72    73    74    75
 *
 *   SEG P   R/S    0     .    +/-    =
 *       7    81    82    83    84    85
 *
 */

/* Private functions */

/** send a 17-bit serial word to TLC5929; bit 17 is in r,  bits 16-1 are in d */
void hw_digit_driver_send_word(unsigned char r, unsigned int d);

/* Private data */

/** map of segments required for each of the LED characters */
/*  Segment weighting follows TMC1500 (E,F,B,G,C,A,D,P} */
const char SEGMENT_MAP[32][8] =
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
    /*^*/ {0, 1, 1, 0, 0, 1, 0, 0}, // 28 A-B-F
    /*.*/ {0, 0, 0, 0, 0, 0, 0, 1}, // 29 DP
    /*-*/ {0, 0, 0, 1, 0, 0, 0, 0}, // 30 G
    /* */ {0, 0, 0, 0, 0, 0, 0, 0}  // 31 <none>
    /*     E  F  B  G  C  A  D  P   */
};


////////////////////////

/** return 0/1 if the specified segment s is to be illuminated in character code c */
char mux57_is_segment(unsigned char c, unsigned char s)
{
    if (c > 31)     // if not a defined character, consider it blank
        c = 31;
    return SEGMENT_MAP[c][s & 0x7];
}

//** parse dA (digits) and dB (mask) contents to determine which digits have segment s illuminated */
unsigned int mux57_which_digits(ti57_reg_t* digits, ti57_reg_t* mask, unsigned char s)
{
    unsigned int d = 0;

    // when segment requested is a-g
    if (s < 7)
    {
        // Go through the 12 digits in descending order
        for (int i = 11; i >= 0; i--)
        {
            char c;
            // Shift the existing d value left one bit
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
    else if (s == 7)
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

/* digit cathode connections to each TLC5929 output
   OUT0  -> d12, OUT1  -> d11, OUT2  -> d10, OUT3  -> d9
   OUT4  -> d8 , OUT5  -> d7 , OUT6  -> d6,  OUT7  -> d5
   OUT8  -> d4 , OUT9  -> d3 , OUT10 -> d2,  OUT11 -> d1
   OUT12...OUT15 should always be off (to enable PS mode)

   outputs are shifted into TLC5929 15...0 order.
   Neither DP of D1 nor D of D12 may EVER be activated
     due to hardware limitations of the TI-57 PCB */

/** parse dA (digits) and dB (mask) contents to determine which TLC5929 outputs should be active for segment s */
unsigned int mux57_which_outputs(ti57_reg_t* digits, ti57_reg_t* mask, unsigned char s)
{
    unsigned int d = 0;

    // when segment requested is a-g
    if (s < 7)
    {
        // Go through the 12 digits in ascending order
        for (int i = 0; i < 12; i++)
        {
            char c;
            // Shift the exsting d value right one bit
            d = d >> 1;
            // Assume the character is only based on the digit information
            c = ((*digits)[i] & 0xf); /*0-f*/
            // Replace the character if necessary based on the mask information
            if ((*mask)[i] & 0x8)
            {
                c = CHAR_CODE_BLANK; /* blank */
            }
            else if ((*mask)[i] & 0x1)
            {
                c = CHAR_CODE_MINUS; /* minus sign */
            }
            // if digit is active, set the msb
            if (SEGMENT_MAP[c][s])
                d = d | (1 << 15);
        }
    }
    // when segment requested is p (decimal point)
    else if (s == 7)
    {
        // Go through the 12 digits in descending order
        for (int i = 0; i < 12; i++)
        {
            // Shift the existing d value right one bit
            d = d >> 1;
            // indicate if this digit has decimal point active
            if ((*mask)[i] & 0x2)
                d = d | (1 << 15);
        }
    }

    /* Segment DP (7) of digit #1 can NEVER be activated */
    if (s == 7)
        d = d & 0xffe0;
    /* Segment D (6) of digit #12 can NEVER be activated */
    else if (s == 6)
        d = d & 0x7ff0;
    /* always clear 4 lsb of d (unused driver outputs) */
    else
        d = d & 0xfff0;

    return d;
}

/**perform a 6.4ms long display update cycle, using DIGITS and MASK */
/* return value is a scancode (or 0) collected during display scanning */
unsigned char hw_display_cycle(ti57_reg_t* digits, ti57_reg_t* mask)
{
    unsigned char segment = 0;  // segment counter 0-7
    unsigned int d_outputs = 0; // digit driver output data
    unsigned int k_inputs = 0;  // keyboard column inputs
    unsigned char scancode = 0; // keyboard scancode

    /* preload the TLC5929 SR with SEGMENT E (#0) digit outputs */
    /* determine which digits have segment E illuminated */
    d_outputs = mux57_which_outputs(*digits, *mask, 0);

    /* send the pattern to the TLC5929 digit driver */
    hw_digit_driver_load(d_outputs);

    /* Wait for SysTick to start segment scan sequence */
    __WFI();

    /* loop through the 8 segments, 800us per segment */
    for (segment = 0; segment < 8; segment++)
    {

        /* disable all segment drive outputs */
        hw_segment_disable_all();

        /* drive previously loaded digit pattern to TLC5929 outputs */
        hw_digit_driver_update();

        /* enable the appropriate segment drive output */
        hw_segment_enable(segment);

        /* at this point in time, one segement is active in all of the
             digit(s) that need it */

        /* wait for SEGMENT_TICKS-1 with segment and digit(s) driven.
             During the last tick the next segment's digit pattern
             will be shifted out (but not yet loaded).*/
        for (int u = SEGMENT_TICKS - 1; u > 0; u--)
            __WFI();

        /* determine which digit pattern to preload for next segment */
        /* or all off (0's) in the case of all segments completed */
        if (segment < 7)
            d_outputs = mux57_which_outputs(*digits, *mask, segment + 1);
        else
            d_outputs = 0;

        /* send the pattern to the TLC5929 digit driver */
        hw_digit_driver_load(d_outputs);

        /* read this segment's keyboard scancode. Save it if no scancode already held */
        if (scancode == 0)
            scancode = hw_read_keyboard_row(segment);

        /* wait for final tick of this segment period */
        __WFI();
    }

    /* disable all segment drive outputs */
    hw_segment_disable_all();

    /* update the TLC5929 to all outputs off (enter power-save mode) */
    hw_digit_driver_update();

    /* Scancode = 100 if Program Manager should be entered - 2ND + INV + CLR */
    if (hw_raw_keyboard_row(0) == 0x13)
        scancode = 100;

    return scancode;
}


/** display a single character at digit 12 in a 6.4ms cycle */
/* return value is a scancode (or 0) collected during display scanning */
unsigned char hw_display_char_d12(unsigned char c)
{
    unsigned char segment = 0;  // segment counter 0-7
    unsigned char digit_en = 0; // digit driver enable
    unsigned char scancode = 0; // keyboard scancode

    /* Wait for SysTick to start segment scan sequence */
    __WFI();

    /* loop through the 8 segments, 800us per segment */
    for (segment = 0; segment < 8; segment++)
    {
        /* disable all segment drive outputs */
        hw_segment_disable_all();

        /* turn on DIG12 if segment is needed for this character */
        if (mux57_is_segment(c, segment))
            GPIOB->BRR = GPIO_Pin_4;    // drive D12DIR low
        else
            GPIOB->BSRR = GPIO_Pin_4;   // float D12DIR high

        /* enable the appropriate segment drive output */
        hw_segment_enable(segment);

        /* wait for SEGMENT_TICKS with segment and digit driven. */
        for (int u = SEGMENT_TICKS; u > 0; u--)
            __WFI();

        /* read this segment's keyboard scancode. Save it if no scancode already held */
        if (scancode == 0)
            scancode = hw_read_keyboard_row(segment);
    }

    /* disable all segment drive outputs */
    hw_segment_disable_all();

    /* turn D12DIR output back off (floating high) */
    GPIOB->BSRR = GPIO_Pin_4;

    return scancode;
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
void hw_segment_enable(unsigned char s)
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

/* TLC5929 Digit Driver specific functions */

/* reset the TLC5929 digit driver LATCH, CLOCK, and DATA signals */
void hw_digit_driver_reset(void)
{
    /* Negate LATCH */
    GPIOA->BRR = GPIO_Pin_15;
    /* Negate SCLK and SDATA */
    GPIOB->BRR = GPIO_Pin_3 | GPIO_Pin_5;
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

/* send output data to the TLC5929 */
void hw_digit_driver_send_word(unsigned char r, unsigned int d)
{
    /* Negate LATCH */
    GPIOA->BRR = GPIO_Pin_15;
    /* Negate SCLK */
    GPIOB->BRR = GPIO_Pin_3;
    /* delay for some LATCH - SCLK setup time */
    __NOP();
    __NOP();
    __NOP();

    /* clock in bit 16 based upon value in argument 'r' */
    if (r & 1u)
        /* LSB of r is 1 --> assert SDAT */
        GPIOB->BSRR = GPIO_Pin_5;
    else
        /* LSB of r is 0 --> negate SDAT */
        GPIOB->BRR = GPIO_Pin_5;

    /* delay for data setup then assert SCLK */
    __NOP();
    __NOP();
    GPIOB->BSRR = GPIO_Pin_3;

    /* delay for data hold then negate SCLK */
    __NOP();
    __NOP();
    GPIOB->BRR = GPIO_Pin_3;

    /* clock in bits 15-0 based upon value in argument 'd' */
    for (int i = 16; i > 0; i--)
    {
        if (d & 0x8000u)
            /* MSB of d is 1 --> assert SDAT */
            GPIOB->BSRR = GPIO_Pin_5;
        else
            /* MSB of d is 0 --> negate SDAT */
            GPIOB->BRR = GPIO_Pin_5;

        /* delay for data setup then assert SCLK */
        __NOP();
        __NOP();
        GPIOB->BSRR = GPIO_Pin_3;

        /* delay for data hold then negate SCLK */
        __NOP();
        __NOP();
        GPIOB->BRR = GPIO_Pin_3;

        /* shift d left by 1 to expose next bit */
        d = d << 1;
    }

    /* always leave with SDATA and SCLK negated */
    GPIOB->BRR = GPIO_Pin_5 | GPIO_Pin_3;
}


/** Update the TLC5929 driver outputs by toggling the LATCH signal */
void hw_digit_driver_update(void)
{
    /* Assert LATCH */
    GPIOA->BSRR = GPIO_Pin_15;
    /* delay for some uncritical hold time before negating LATCH */
    __NOP();
    __NOP();
    GPIOA->BRR = GPIO_Pin_15;
}

/* Shift a serial word into the TLC5929 output driver shift register */
void hw_digit_driver_load(unsigned int d)
{
    hw_digit_driver_send_word(0, d);
}

/* encode the K1-K5 key column inputs and active segment into a scancode */
unsigned char hw_read_keyboard_row(unsigned char seg)
{
    unsigned char code = 0;
    unsigned char row;
    unsigned char col;
    unsigned int kin;

    /* read the K1-K5 switch inputs */
    kin = (GPIO_ReadInputData(GPIOA) & 0x001f);

    /* save to "raw data" array */
    raw_keyboard_inputs[seg & 0x7] = kin;

    /* convert segment number (0-7) to row number (1-8) */
    row = (seg & 0x7) + 1;

    /* determine column number from kin bits */
    if (kin & 1)
        col = 1;
    else if (kin & 2)
        col = 2;
    else if (kin & 4)
        col = 3;
    else if (kin & 8)
        col = 4;
    else if (kin & 16)
        col = 5;
    else
    {
        col = 0;
        row = 0;
    }

    /* code will be either 0 or a key scancode (11..85) */
    code = ((row << 4) | col);
    return code;
}

/* return the K1-K5 key column inputs obtained through read_keyboard_row() */
unsigned char hw_raw_keyboard_row(unsigned char s)
{
    return raw_keyboard_inputs[s & 0x7];
}
