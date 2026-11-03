/* Never stops the watchdog: it should reset the chip every 32768 SMCLK cycles (about 33 ms at 1 MHz).
 * boots counts resets in FRAM (written with write protection off); last_cause keeps SYSRSTIV. */
#include <msp430.h>
#include <stdint.h>
__attribute__((section(".persistent"))) volatile uint16_t boots = 0;
__attribute__((section(".persistent"))) volatile uint16_t last_cause = 0;
int main(void) {
    uint16_t cause = SYSRSTIV;
    SYSCFG0 = FRWPPW;               // allow FRAM writes
    boots++;
    last_cause = cause;
    SYSCFG0 = FRWPPW | PFWP;
    for (;;) ;
}
