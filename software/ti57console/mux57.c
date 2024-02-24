#include "mux57.h"
#include <stdio.h>

/*
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
 * {a,b,c,d,e,f,g,p}
 */

/* map of segments required for each of the LED digit types */



static char MUX_MAP[18][8] = {
  /*0*/ {1,1,1,1,1,1,0,0}, //A-B-C-D-E-F
	/*1*/ {0,1,1,0,0,0,0,0}, //B-C
  /*2*/ {1,1,0,1,1,0,1,0}, //A-B-D-E-G
	/*3*/ {1,1,1,1,0,0,1,0}, //A-B-C-D-G
  /*4*/ {0,1,1,0,0,1,1,0}, //B-C-F-G
	/*5*/ {1,0,1,1,0,1,1,0}, //A-C-D-F-G
  /*6*/ {1,0,1,1,1,1,1,0}, //A-C-D-E-F-G
	/*7*/ {1,1,1,0,0,0,0,0}, //A-B-C-D-E-F
  /*8*/ {1,1,1,1,1,1,1,0}, //A-B-C-D-E-F-G
	/*9*/ {1,1,1,1,0,1,1,0}, //A-B-C-D-F-G
	/*A*/ {1,1,1,0,1,1,1,0}, //A-B-C-E-F-G
	/*b*/ {0,0,1,1,1,1,1,0}, //C-D-E-F-G
	/*c*/ {0,0,0,1,1,0,1,0}, //D-E-G
	/*d*/ {0,1,1,1,1,0,1,0}, //B-C-D-E-G
	/*E*/ {1,0,0,1,1,1,1,0}, //A-D-E-F-G
	/*F*/ {1,0,0,0,1,1,1,0}, //A-E-F-G
	/* */ {0,0,0,0,0,0,0,0}, //<none>
	/*-*/ {0,0,0,0,0,0,1,0}  //G
};

char mux57_is_segment(unsigned char c, unsigned char s)
{
	if (c > 16) 
	    c = 17;
    return MUX_MAP[c][s & 0x7];
}

unsigned short mux57_which_digits(ti57_reg_t *digits, ti57_reg_t *mask, unsigned char s)
{
    unsigned short d = 0;
	
	// when segment requested is a-g
    if (s < 7) {
		// Go through the 12 digits in descending order
		for (int i = 11; i >= 0; i--) {
			char c;			
			// Shift the exsting d value left one bit
			d = d << 1;
			// Assume the character is only based on the digit information
			c = ((*digits)[i] & 0xf); /*0-f*/
			// Replace the character if necessary based on the mask information
			if ((*mask)[i] & 0x8) {
				c = 16; /* blank */
			} else if ((*mask)[i] & 0x1) {
				c = 17; /* minus sign */
			}
			// OR in this digit active status (0 or 1)
			d = d | MUX_MAP[c][s];
		}
	}
	// when segment requested is p (decimal point)
	else if (s == 7) {
		// Go through the 12 digits in descending order
		for (int i = 11; i >= 0; i--) {		
			// Shift the exsting d value left one bit
			d = d << 1;
			// indicate if this digit has decimal point active
			if ((*mask)[i] & 0x2)
				d = d | 1;
		}
	}
    return d;
}

/* digit cathode connections to each CAT4106 output
   LED1  -> d11, LED2  -> d10, LED3  -> d9, LED4  -> d8
   LED5  -> d7 , LED6  -> d6 , LED7  -> d4, LED8  -> d5
   LED9  -> d2 , LED10 -> d3 , LED11 -> d1, LED12 -> Z
   LED13 -> **, LED14 -> d12,  LED15 -> X,  LED16 -> X

   ** = onboard LED indicator
    X = no LED attached
    Z = must always be off

   outputs are shifted into CAT4016 16...1 order. 
   Special considerations: neither DP of D1 nor D of D12
   can EVER be activated! */

/* Array to map LED output (index) to LED digit (value)
   0 = always off
   1-12 is LED digit position (1 = RHS digit)
   >12 = always on */

static char DIGIT_MAP[16] = {11, 10, 9, 8, 7, 6, 4, 5, 2, 3, 1, 0, 16, 12, 0, 0}; 

unsigned short mux57_which_outputs(ti57_reg_t *digits, ti57_reg_t *mask, unsigned char s)
{
    unsigned short d = 0;
	
	printf("\n\r-----\n\rSegment %d ",s);
	// when segment requested is a-g
    if (s < 7) {
		// Go through the 16 CAT4016 driver outputs
		for (int i = 15; i >= 0; i--) {
			char c, j;			
			// Shift the exsting d value left one bit
			d = d << 1;
			// Determine which digit, if any, should be considered
			j = DIGIT_MAP[i];
			printf("\n\r %d bit is digit %d ",i,j);
      // If this bit position corresponds to a digit, process it
			if (j>0) 
			{
				// Assume the character is only based on the digit information
				c = ((*digits)[j-1] & 0xf); /*0-f*/
				printf("dA[%d] is %d ",j-1,c);
				// Replace the character if necessary based on the mask information
				if ((*mask)[j-1] & 0x8) {
					c = 16; /* blank */
				} else if ((*mask)[j-1] & 0x1) {
					c = 17; /* minus sign */
				}
				printf(" value is %d ",c);
				// OR in this digit active status (0 or 1)
				d = d | MUX_MAP[c][s];
			}
			// if this bit position is "always on" shift in a 1
			if (j>12)
				d = d | 1;
			printf("on/off is %d ",MUX_MAP[c][s]);
			printf("new d is %03X ",d);
		}
	}
	// when segment requested is p (decimal point)
	else if (s == 7) {
		// Go through the 16 CAT4016 driver outputs
		for (int i = 15; i >= 0; i--) {		
			char j;
			// Shift the exsting d value left one bit
			d = d << 1;
			// Determine which digit, if any, should be considered
			j = DIGIT_MAP[i];
      // If this bit position corresponds to a digit, process it
			if (j>0) 
			{
				// indicate if this digit has decimal point active
				if ((*mask)[j] & 0x2)
					d = d | 1;
			}
			// if this bit position is "always on" shift in a 1
			if (j>12)
					d = d | 1;
		}
	}	
		/* Segment DP of digit #1 and segment D of digit #12 can NEVER be activated 
	     Because of hardware limitations of how the display is connected to the
	     TMC1500. */
	
		// segment DP
		if (s==7)
			d = d & ~(1<<5);
		// segment D
		else if (s==3)
			d = d & ~(1<<2);
	
    return d;
}

