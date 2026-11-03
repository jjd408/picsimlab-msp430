/* Clock system: DCO to 8 MHz with the FLL on REFO (as TI's examples do), MCLK = 8 MHz, SMCLK = 4 MHz.
 *   results[0]: CSCTL7 after the FLL locks (expect XT1OFFG only: ACLK still asks for XT1, there's no crystal)
 *   results[1]: watchdog interval interrupts on SMCLK / 32768 in the first second (expect ~122)
 *   results[2]: the same, counted for 1 s while main sleeps in LPM3, where SMCLK stops (expect 0)
 *   results[3]: 0x600D when finished */
#include <msp430.h>
#include <stdint.h>

volatile uint16_t results[4];
volatile uint16_t wdt_count;

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    __bis_SR_register(SCG0);                 // FLL off while changing it
    CSCTL3 |= SELREF__REFOCLK;
    CSCTL1 = DCORSEL_3;                      // 8 MHz range
    CSCTL2 = FLLD_0 + 243;                   // DCOCLKDIV = 244 * 32768 = 7.995 MHz
    __delay_cycles(3);
    __bic_SR_register(SCG0);                 // FLL on
    while (CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1))
        ;
    CSCTL7 &= ~DCOFFG;
    CSCTL5 = DIVM__1 | DIVS__2;              // MCLK 8 MHz, SMCLK 4 MHz
    results[0] = CSCTL7;

    WDTCTL = WDTPW | WDTSSEL__SMCLK | WDTTMSEL | WDTCNTCL | WDTIS__32K;
    SFRIE1 |= WDTIE;
    __enable_interrupt();
    __delay_cycles(8000000);                 // 1 s at 8 MHz
    results[1] = wdt_count;

    wdt_count = 0;
    P1DIR |= BIT0;
    // Sleep in LPM3; the ACLK-driven WDT can't run (it's on SMCLK), so use a busy timer: wake via P1.1
    P1REN |= BIT1;
    P1OUT |= BIT1;
    P1IES |= BIT1;
    P1IFG = 0;
    P1IE |= BIT1;
    __bis_SR_register(LPM3_bits | GIE);      // the runner pulls P1.1 low after 1 s in here
    results[2] = wdt_count;
    results[3] = 0x600D;
    for (;;) ;
}

__attribute__((interrupt(WDT_VECTOR))) void wdt_isr(void) {
    wdt_count++;
}

__attribute__((interrupt(PORT1_VECTOR))) void port1_isr(void) {
    P1IFG = 0;
    __bic_SR_register_on_exit(LPM3_bits);
}
