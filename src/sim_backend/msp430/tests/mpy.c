/* Multiplies through whatever multiplier the compiler targets (-mhwmult=f5series uses MPY32,
 * -mhwmult=none uses software). out[] holds the results; done is 0x600D at the end. */
#include <msp430.h>
#include <stdint.h>

volatile uint32_t out[12];
volatile uint64_t out64[3];
volatile uint16_t done;

volatile uint16_t a16 = 0xBEEF, b16 = 0x1234;
volatile int16_t s16 = -1234, t16 = 5678;
volatile uint32_t a32 = 0xDEADBEEF, b32 = 0x01234567;
volatile int32_t s32 = -987654321, t32 = 123456;
volatile uint8_t c8 = 200, d8 = 177;
volatile int8_t e8 = -100, f8 = 77;

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    out[0] = (uint32_t)a16 * b16;           // 16x16 unsigned -> 32
    out[1] = (int32_t)s16 * t16;            // 16x16 signed -> 32
    out[2] = (uint16_t)(a16 * b16);         // 16x16 -> 16
    out[3] = a32 * b32;                     // 32x32 -> 32
    out[4] = (uint32_t)(s32 * t32);         // signed 32x32 -> 32
    out[5] = (uint32_t)(c8 * d8);           // 8x8
    out[6] = (uint32_t)(int32_t)(e8 * f8);
    out[7] = (uint32_t)a16 * a32;           // mixed widths
    out[8] = (uint32_t)((int32_t)s16 * s32);
    uint32_t acc = 0;
    for (uint16_t i = 1; i < 20; i++)
        acc += (uint32_t)i * i * 2654435761u;  // multiply-accumulate pattern
    out[9] = acc;
    out[10] = (uint32_t)(t32 / 37);         // division (no hardware support; sanity)
    out[11] = (uint32_t)((uint64_t)a32 * b32 >> 32);  // high word of a 64-bit product
    out64[0] = (uint64_t)a32 * b32;
    out64[1] = (uint64_t)((int64_t)s32 * t32);
    out64[2] = (uint64_t)a32 * a32 * 3;
    done = 0x600D;
    for (;;) ;
}
