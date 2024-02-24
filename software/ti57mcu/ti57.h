/**
 * API for a "lite" version of a TI-57 Emulator
 *
 * "lite" in terms of suitable for a microcontroller
 * e.g. STM32F103 72MHz ARM Cortex M3 with 128KB Flash and 20KB RAM
 *
 * This is derived from xxxxxxxxxxx's excellent and more portable
 * TI-57 emulator, originally targetted to iOS
 */

/**
 * API for clients that want to implement a TI-57 emulator.
 *
 * Sample implementation:
 *   Init:
 *     ti57_t ti57;
 *     ti57_init(&ti57);
 *     
 *   while(1) {
 *     n = ti57_next(&ti57);
 *     *wait for n*[???] to elapse*
 *     update_display(ti57_get_display(&ti57))
 *   On key press:
 *     ti57_key_press(&ti57, row, col);
 *   On key release:
 *     ti57_key_release(&ti57);
 *   }
 */

#ifndef ti57_h
#define ti57_h

#include "state57.h"

/** Initializes the state of a TI-57. */
void ti57_init(ti57_t *ti57);

/**
 * Executes the operation at the current program counter address.
 *
 * A TI-57 is always executing operations, possibly just polling for user
 * input. It takes around 1/5000 seconds (200us) to execute most operations.
 *
 * Returns the relative cost of the operation, most often 1 though some
 * operations, such as those involving the display, may take longer.
 */
int ti57_next(ti57_t *ti57);

/** Should be called when a key is pressed (row in 1..8, col in 1..5). */
void ti57_key_press(ti57_t *ti57, int row, int col);

/** Should be called when a key is released. */
void ti57_key_release(ti57_t *ti57);

/**
 * Returns the display as a string.
 *
 * The display is composed of 12 LEDs and each one is represented by 1
 * character (or 2 characters if there is an additional dot).
 *
 * Characters:
 * - blank character
 * - numbers are represented using:  0..9 - (and dot)
 * - A b C d E F may appear is rare situations
 *
 * For example: "  -3.14159   ".
 */
char *ti57_get_display(ti57_t *ti57);

#endif  /* !ti57_h */
