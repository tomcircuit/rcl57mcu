/**
 * Internal state of a TI-57 and API to decode it.
 */

#ifndef cat4016_h
#define cat4016_h

/* TI-57 EMU hardware specific prototypes */

void hw_cat4016_latch_negate(void);
void hw_cat4016_latch_assert(void);
void hw_cat4016_blank_negate(void);
void hw_cat4016_blank_assert(void);
void hw_cat4016_initialize(void);
void hw_cat4016_send_data_blocking(unsigned short d);
void hw_cat4016_send_data_nonblocking(unsigned short d);
void hw_cat4016_update_outputs(int delay);



#endif  /* !cat4016_h */