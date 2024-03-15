#ifndef progman_h
#define progman_h
#include <stdbool.h>
#include "rcl57mcu.h"

// A program entry structure
typedef struct program_s
{
		unsigned char status;				 // "status byte" == 0=free, 1=used
    unsigned char bytes[50];     // the 50 program bytes
    unsigned short hash;         // Fletcher16 hash over the 50 bytes
} program_t;

#define SLOT_STATUS_FREE (0)
#define SLOT_STATUS_OCCUPIED (1)



/** Program Manager mode */
void mode_progman(void);

/* following constants configure the "Program Manager" mode, which
 * is entered with a special keyboard combination. */

#endif /* progman_h */



