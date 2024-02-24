#include <stdbool.h>
#include <string.h>

/* Helper macros */
#define HEX__(n) 0x##n##LU
#define B8__(x) ((x&0x0000000FLU)?1:0) \
+((x&0x000000F0LU)?2:0) \
+((x&0x00000F00LU)?4:0) \
+((x&0x0000F000LU)?8:0) \
+((x&0x000F0000LU)?16:0) \
+((x&0x00F00000LU)?32:0) \
+((x&0x0F000000LU)?64:0) \
+((x&0xF0000000LU)?128:0)

/* User macros */
#define B8(d) ((unsigned char)B8__(HEX__(d)))
#define B16(dmsb,dlsb) (((unsigned short)B8(dmsb)<<8) \
+ B8(dlsb))
#define B32(dmsb,db2,db3,dlsb) (((unsigned long)B8(dmsb)<<24) \
+ ((unsigned long)B8(db2)<<16) \
+ ((unsigned long)B8(db3)<<8) \
+ B8(dlsb))


static int LEDS_MAP[256];

static void init(void) {
    memset(LEDS_MAP, 0, 256 * sizeof(int));

    LEDS_MAP[' '] = B16(00000000,00000000);

    // Digits.
    LEDS_MAP['0'] = B16(00110001,00100011);
    LEDS_MAP['1'] = B16(00000001,00000010);
    LEDS_MAP['2'] = B16(00100001,11100001);
    LEDS_MAP['3'] = B16(00100001,11000011);
    LEDS_MAP['4'] = B16(00010001,11000010);
    LEDS_MAP['5'] = B16(00110000,11000011);
    LEDS_MAP['6'] = B16(00110000,11100011);
    LEDS_MAP['7'] = B16(00100001,00000010);
    LEDS_MAP['8'] = B16(00110001,11100011);
    LEDS_MAP['9'] = B16(00110001,11000011);
}

int leds57_get_segments(unsigned char c) {
    static bool initialized = false;

    if (!initialized) {
        init();
        initialized = true;
    }
    return LEDS_MAP[c];
}
