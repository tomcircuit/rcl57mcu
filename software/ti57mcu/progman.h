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

#ifndef progman_h
#define progman_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


/* TI-57 program storage using external EEPROM */

/* The 11AA080 EEPROM provides 1024 bytes of storage. This storage
   is used for both TI-57 "user program" storage, as well as nonvolatile
   storage of RCL57 configuration values. The 1024 bytes of EEPROM are
   used in the following fashion:

   OFFSET   LENGTH  DESCRIPTION
   ------   ------  -----------
    000h       1    LED brightness level (1-23)
    001h       3    unused
    004h      33h   Status and Program Step bytes for slot #1
    037h      33h   Status and Program Step bytes for slot #2
    06Ah      33h   Status and Program Step bytes for slot #3
    09Dh      33h   Status and Program Step bytes for slot #4
    ...
    39Ah      33h   Status and Program Step bytes for slot #19
    3CDh      33h   Status and Program Step bytes for slot #20

*/

/** Program Slot status enum */
typedef enum slot_status_e
{
    OPEN,     		// slot is available & has no content
    OCCUPIED, 		// slot is occupied & unlocked
    LOCKED    		// slot is occupied & locked
} slot_status_t;

/** User Program structure */
typedef struct user_program_s
{
    slot_status_t status;           // the status for this slot	
    unsigned char step[50];         // the 50 program step bytes
    unsigned int hash               // the Fletcher hash for this slot
} user_program_t;

/** populate a program structure from an EEPROM buffer

#endif  // progman.h