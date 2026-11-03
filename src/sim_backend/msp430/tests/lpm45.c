/* LPM4.5: first boot sets P1.0 high, arms a P1.1 falling-edge wake and powers down. The wake is a reset:
 * the second boot records SYSRSTIV (0x08 = LPMx.5 wake-up), PMMIFG's LPM5 flag, and P1IFG (the wake pin),
 * in FRAM, then stops. boots counts boots. */
#include <msp430.h>
#include <stdint.h>

__attribute__((section(".persistent"))) volatile uint16_t boots = 0;
__attribute__((section(".persistent"))) volatile uint16_t wake_cause = 0;
__attribute__((section(".persistent"))) volatile uint16_t wake_pmmifg = 0;
__attribute__((section(".persistent"))) volatile uint16_t wake_p1ifg = 0;

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    uint16_t cause = SYSRSTIV;
    SYSCFG0 = FRWPPW;
    boots++;

    // Port settings come back from reset: set them again before unlocking, as TI requires
    P1OUT = BIT0 | BIT1;
    P1DIR = BIT0;
    P1REN = BIT1;
    P1IES = BIT1;

    if (cause == SYSRSTIV_LPM5WU) {
        wake_cause = cause;
        wake_pmmifg = PMMIFG;
        wake_p1ifg = P1IFG;
        SYSCFG0 = FRWPPW | PFWP;
        PM5CTL0 &= ~LOCKLPM5;
        for (;;) ;
    }
    SYSCFG0 = FRWPPW | PFWP;
    PM5CTL0 &= ~LOCKLPM5;
    P1IFG = 0;
    P1IE = BIT1;
    PMMCTL0_H = PMMPW_H;
    PMMCTL0_L |= PMMREGOFF;
    __bis_SR_register(LPM4_bits | GIE);
    for (;;) ;
}
