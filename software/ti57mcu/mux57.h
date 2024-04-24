#ifndef mux57_h
#define mux57_h

#include "rcl57mcu.h"
#include <stdint.h>


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
 *   SEG E   2ND   INV   LNx   CE    CLR
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
 * The keyboard is scanned simultaneously with the display - each segment
 * output (active high) is connected to a row of keyboard switches. Thus,
 * up to 8 rows of switches are supported. Each column of keyboard switches
 * is connected to a column sense line, K1-K5. As each segment line is
 * driven high, the K1-K5 inputs should be read. Any K input that is high
 * indicates that a keyboard switch is depressed.
 */

/* Assumes 20 KHz SysTick frequency (50us per tick) */

#define CHAR_CODE_POINT (29)
#define CHAR_CODE_MINUS (30)
#define CHAR_CODE_BLANK (31)

/* array type that is digit and mask register */
typedef uint8_t display_data_t[16];

/****************************************/
/* LED Display Cycle related prototypes */
/****************************************/

/** do a complete display cycle and return key scancode */
uint8_t hw_display_cycle(display_data_t* digits, display_data_t* mask);

/** do a complete display cycle of a single character at digit 12 and return key scancode */
uint8_t hw_display_char_d12(uint8_t c);

/** wait for key release - t * 10ms "debounce" */
void hw_wait_for_key_release(uint32_t t);

/* convert null-terminated string to arrays of display digit and mask codes */
void mux57_paint_digits(const uint8_t* ins, display_data_t* digits, display_data_t* mask);

/* convert digit display and mask codes to null-terminated string */
uint8_t* mux57_display_to_str(display_data_t* digits, display_data_t* mask);

/* display a string to LED for n-display cycles */
void mux57_splash(const uint8_t* ins, unsigned int n);

/**************************************************/
/* LED display segment driver releated prototypes */
/**************************************************/

/** enable all segment driver PMOS Q's*/
void hw_segment_enable_all(void);

/** disable all segment driver PMOS Q's*/
void hw_segment_disable_all(void);

/** enable one segment driver PMOS Q */
void hw_segment_enable(uint8_t s);

/*************************************************************/
/* TLC5929 LED Digit Driver related constants and prototypes */
/*************************************************************/

/* The TLC5929 performs the function of the digit driver. It is a
 * serially-controlled 16-channel constant current sink. Each digit
 * cathodes is connected to one of the TLC5929 outputs. Because at
 * most only one segment per digit is active at a time, each output
 * needs only to sink approx. 5mA. The TLC5929 was chosen because
 * it has a power-save mode; when all digit outputs are off (0) the
 * device consumes less than 40uA of current. Most other serial LED
 * drivers (e.g. CAT4016) consume several mA of static current at all
 * times. */

/* TLC5929 control word value:
   b.15 = 1  to enable Power Save mode
     b.0-6 = 64 to set to ~5mA segment sink current */
#define TLC5929_CONTROL ((1 << 15) + 65)

/** Initialize the TLC5929 digit driver IC by turning all outputs off,
 *  setting Global Brightness level to 50%, and enabling Power Save */
void hw_digit_driver_initialize(void);

/** Shift a serial word into the TLC5929 output driver register */
void hw_digit_driver_load(uint16_t d);

/** Shift a serial word into the TLC5929 configuration register */
void hw_digit_driver_config(uint16_t d);

/** Set the display dimming via a 'linearized' 0-23 value */
void hw_digit_driver_intensity(uint8_t i);

/** Update the TLC5929 driver outputs by toggling the LATCH signal */
void hw_digit_driver_update(void);

#endif /* mux57_h */



