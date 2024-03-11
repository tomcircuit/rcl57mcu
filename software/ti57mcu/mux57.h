#ifndef mux57_h
#define mux57_h

#include "state57.h"

/**
 * Support for TI-57 multidigit LED display and keyboard hardware
 * March 2024, Tom LeMense
 *
 * The calculator display is composed of 12 LED digits, digit 12 is the
 * leftmost digit, and digit 1 is the rightmost digit:
 *
 *   [D12] [D11] [D10] [D9] ... [D4] [D3] [D2] [D1]
 *
 *   D12 is sign of the mantissa and typically blank or a "-"
 *   D3 is sign of the exponent and typically blank or a "-"
 *   D2,D1 are typically the exponent or blank
 *
 * Each digit has 8 segments (a-g,p):
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
 * The digits are "multiplexed" in that all "a" segments are connected
 * together, all "b" segments are connected together, etc. To illuminate
 * a specific segment on a specific digit, the segment line must be brought
 * to a positive voltage, and the individual digit line should be driven
 * to a voltage near ground potential (it's a bit more complicated than
 * this, but more-or-less this is what is going on).
 *
 * Due to hardware limitations, digit 1 (rightmost) cannot illuminate the
 * "p" segment, and digit 12 (leftmost) cannot illuminate the "d" segment.
 *
 * The TMC1500 scans the display by SEGMENTS rather than DIGITS, for
 * reasons specific to the PMOS technology in which it was fabricated.
 * Refer to US Patent 4,014,012 "Segment scanning method for calculator
 * display system" (1975) for more details. TI economized on transistor
 * count by using an LFSR, rather than a binary counter, as the segment
 * scan counter. The TMC1500 segment counter counts: 7,6,5,2,4,0,1,3.
 * I have not made any attempt to preserve this aspect, as it really
 * should not matter in which ORDER the segments are scanned.
 *
 * Ignoring the ORDER in which the segments are scanned is fine, but
 * it is helpful to preserve the MAPPING between the segment number
 * (0-7) and the LED display segment (a-g,dp). Here is the segment
 * vs. number scheme used in the TMC1500 and implemented here as well:
 *
 *   SEG # --  0   1   2   3   4   5   6   7
 *   LED   --  E   F   B   G   C   A   D   DP
 *
 * During display cycles (8x longer than normal cycles) each segement
 * line is, in turn, brought to a positive voltage, and ALL of the digit
 * line(s) in which that segment is to be illuminated are brought to near
 * ground potential. In the TMC1500-based TI-55 I examined, each segement
 * line is driven for 800us, and therefore a complete Display Cycle is
 * 6.4ms (8 segment lines, one at a time, each for 800us).
 *
 * The keyboard is connected as shown here, with each ROW being driven
 * by the segment driver ouput as the display is scanned, and each COLUMN
 * being read during that segment's active period. By keeping the strange
 * segment numbering (described above) the keyscan codes are easy to
 * generate - the ROW is segment # plus one, and the column is the encoding
 * of "one hot" K1..K5 inputs.
 *
 *            K     K     K     K     K
 *            1     2     3     4     5
 *
 *   SEG E   2ND   INV   LNX   CE    CLR
 *            11    12    13    14    15  <--- scancode
 *
 *   SEG F   LRN   X/T   X^2   vX    1/X
 *            21    22    23    24    25
 *
 *   SEG B   SST   STO   RCL   SUM   Y^X
 *            31    32    33    34    35
 *
 *   SEG G   BST   EE     (     )     /
 *            41    42    43    44    45
 *
 *   SEG C   GTO    7     8     9     X
 *            51    52    53    54    55
 *
 *   SEG A   SBR    4     5     6     -
 *            61    62    63    64    65
 *
 *   SEG D   RST    1     2     3     +
 *            71    72    73    74    75
 *
 *   SEG P   R/S    0     .    +/-    =
 *            81    82    83    84    85
 *
 * The keyboard is scanned simultaneously with the segments - each segment
 * output (active high) is connected to a row of keyboard switches. Thus,
 * up to 8 rows of switches are supported. Each column of keyboard switches
 * is connected to a column sense line, K1-K5. As each segment line is
 * driven high, the K1-K5 inputs should be read. Any K input that is high
 * indicates that a keyboard switch is depressed.
 */

/* Assumes 20 KHz SysTick frequency (50us per tick) */

#define DISPLAY_MASK_BLANK  (0x8)
#define DISPLAY_MASK_MINUS  (0x1)
#define DISPLAY_MASK_POINT  (0x2)

#define CHAR_CODE_BLANK (31)
#define CHAR_CODE_MINUS (30)
#define CHAR_CODE_POINT (29)

/** raw keyboard K1-K5 data for each row */
static unsigned char raw_keyboard_inputs[8];

/** return 0/1 if the specified segment s is to be illuminated in character code c */
char mux57_is_segment(unsigned char c, unsigned char s);

/** parse dA (digits) and dB (mask) contents to determine which digits have
 * segment s illuminated
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
 */
unsigned int mux57_which_digits(ti57_reg_t* digits, ti57_reg_t* mask, unsigned char s);

/** parse dA (digits) and dB (mask) contents to determine which digit outputs
 * should have segment s illuminated.
 *
 * The return value contains '1' in the bit positions corresponding to the
 * outputs (e.g. LED1 is bit 0, LED2 is bit 1, etc) that must be driven.
 *
 * TLC5929 Output    :  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
 * LED Display Digit : 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1
 *
 * TLC5929 outputs 12-15 are not used and should be off (0 in output register)
 *
 */
unsigned int mux57_which_outputs(ti57_reg_t* digits, ti57_reg_t* mask, unsigned char s);

/*************************************************/
/* RCL-57 PCB V2 LED Segment releated prototypes */
/*************************************************/

/** disable all segment driver PMOS Q's*/
void hw_segment_disable_all(void);

/** enable all segment driver PMOS Q's*/
void hw_segment_enable_all(void);

/** enable one segment driver PMOS Q */
void hw_segment_enable(unsigned char s);

/** do a complete display cycle and return key scancode */
unsigned char hw_display_cycle(ti57_reg_t* digits, ti57_reg_t* mask);

/** display a single character at digit 12 */
unsigned char hw_display_char_d12(unsigned char c);

/** determine key scancode from K1-K5 inputs and specified segment */
unsigned char hw_read_keyboard_row(unsigned char seg);

/** return the K1-K5 key column inputs obtained during hw_display_cycle or hw_display_char */
unsigned char hw_raw_keyboard_row(unsigned char s);

/*************************************************************/
/* TLC5929 LED Digit Driver related constants and prototypes */
/*************************************************************/

/* TLC5929 control word value:
   b.15 = 1  to enable Power Save mode
     b.0-6 = 64 to set to ~5mA segment sink current */
#define TLC5929_CONTROL ((1 << 15) + 64)

/* the TLC5929 is used as the digit driver (current sink). It is a
 * serially-controlled 16-channel constant current sink. Each output is
* connected to one of the digit cathodes. Because at most only one segment
* per digit is active at a time, each output needs only to sink approx.
* 5mA. The TLC5929 was chosen because it has a power-save mode; when all
* digit outputs are off (0) the device consumes less than 40uA of current.
* Most other serial LED drivers (e.g. CAT4016) consume several mA of static
* current at all times.

/** reset the TLC5929 digit driver LATCH, CLOCK, and DATA signals */
void hw_digit_driver_reset(void);

/** Initialize the TLC5929 digit driver IC by turning all outputs off,
 *  setting Global Brightness level to 50%, and enabling Power Save */
void hw_digit_driver_initialize(void);

/** Shift a serial word into the TLC5929 output driver shift register */
void hw_digit_driver_load(unsigned int d);

/** Update the TLC5929 driver outputs by toggling the LATCH signal */
void hw_digit_driver_update(void);

#endif /* mux57_h */



