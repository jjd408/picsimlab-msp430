/* Timer_B up/down mode: TB1 on SMCLK counts 0..500..0; TB1.1 on P2.0 in toggle/set mode with CCR1 = 125
 * toggles at 125 on the way up and down and is set at the top, so it's high from 125 up to 125 down:
 * 750 of every 1000 counts. CCR1 changes go through the CLLD = 1 latch (loaded at zero). */
#include <msp430.h>

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    P2SEL0 |= BIT0;
    P2DIR |= BIT0;
    PM5CTL0 &= ~LOCKLPM5;
    TB1CCR0 = 500;
    TB1CCTL1 = OUTMOD_6 | CLLD_1;
    TB1CCR1 = 125;
    TB1CTL = TBSSEL__SMCLK | MC__UPDOWN | TBCLR;
    __bis_SR_register(LPM0_bits);
    for (;;) ;
}
