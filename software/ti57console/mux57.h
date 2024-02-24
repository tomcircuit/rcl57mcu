#ifndef mux57_h
#define mux57_h

#include "state57.h"

/**
 * Support for TI-57 multidigit LED display hardware 
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
 * The TMC1500 scans the display by SEGMENTS rather than DIGITS, for
 * reasons specific to the PMOS technology in which it was fabricated.
 * Refer to US Patent 4,014,012 "Segment scanning method for calculator 
 * display system" (1975) for more details.
 *
 * During display cycles (8x longer than normal cycles) each segement 
 * line is, in turn, brought to a positive voltage, and ALL of the digit 
 * line(s) in which that segment is to be illuminated are brought to near 
 * ground potential. In the TMC1500-based TI-55 I examined, each segement 
 * line is driven for 800us, and therefore a complete Display Cycle is 
 * 6.4ms (8 segment lines, one at a time, each for 800us).
 */
 

/** Indicates if a LED segement is active for 0-9,A-F,' ' and '-' */
char mux57_is_segment(unsigned char c, unsigned char s);

/**
 * Indicates for which digits a specific segment should be illuminated to
 * obtain the digit display specified within ti57->dA and ti57->dB.
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
 *                         111000000000
 *                         210987654321 --> digit # (there are only 12 digits!)
 */
unsigned short mux57_which_digits(ti57_reg_t *digits, ti57_reg_t *mask, unsigned char s);

/**
 * Indicates which CAT4016 outputs should be active for a specific segment to
 * obtain the digit display specified within ti57->dA and ti57->dB.
 *
 * The return value contains '1' in the bit positions corresponding to the 
 * outputs (e.g. LED1 is bit 0, LED2 is bit 1, etc) that must be driven. 
 *
 * CAT4016 Output    :  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16 
 * LED Display Digit : 10,  9,  8,  7,  6,  5,  3,  4,  1,  2,  0,  X,  X, 11,  X   X
 *
 * where 'X' means 'always leave this bit 0'
 * 
 */
unsigned short mux57_which_outputs(ti57_reg_t *digits, ti57_reg_t *mask, unsigned char s);

/* Map from segment to STM32F port B output */
unsigned short mux57_portb_segment(unsigned char s);


#endif /* mux57_h */



