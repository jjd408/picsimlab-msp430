/* Timer_B test. Main sleeps in LPM0, so ACLK and SMCLK both run.
 *   TB2: EXTCOMIN as the handheld will make it: ACLK, up mode, CCR0 = 272 (32768 / 273 = 120 Hz),
 *        TB2.1 in toggle mode on P5.0, so a 60 Hz square wave.
 *   TB1: PWM: SMCLK, up mode, CCR0 = 999, CCR1 = 250 in reset/set mode: 25% duty on P2.0 (TB1.1).
 *   TB0: continuous mode on ACLK; CCR0 interrupt every 3277 counts (10 Hz);
 *        CCR1 captures a rising edge on P1.6 (TB0.CCI1A).
 *   TB3: up mode on ACLK, CCR0 = 327 (100 Hz); CCR1 and TBIFG interrupts through TB3IV.
 * results[]: TB0 CCR0 interrupts, TB3 CCR1 interrupts, TB3 TBIFG interrupts, captured TB0R, captures, 0x600D */
#include <msp430.h>
#include <stdint.h>

volatile uint16_t results[6];

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;

    P5SEL0 |= BIT0;              // P5.0 = TB2.1
    P5DIR |= BIT0;
    P2SEL0 |= BIT0;              // P2.0 = TB1.1
    P2DIR |= BIT0;
    P1SEL1 |= BIT6;              // P1.6 = TB0.CCI1A (input)
    P1DIR &= ~BIT6;
    PM5CTL0 &= ~LOCKLPM5;

    TB2CCR0 = 272;
    TB2CCR1 = 100;
    TB2CCTL1 = OUTMOD_4;
    TB2CTL = TBSSEL__ACLK | MC__UP | TBCLR;

    TB1CCR0 = 999;
    TB1CCR1 = 250;
    TB1CCTL1 = OUTMOD_7;
    TB1CTL = TBSSEL__SMCLK | MC__UP | TBCLR;

    TB0CCR0 = 3277;
    TB0CCTL0 = CCIE;
    TB0CCTL1 = CM__RISING | CCIS__CCIA | CAP | CCIE;
    TB0CTL = TBSSEL__ACLK | MC__CONTINUOUS | TBCLR;

    TB3CCR0 = 327;
    TB3CCR1 = 100;
    TB3CCTL1 = CCIE;
    TB3CTL = TBSSEL__ACLK | MC__UP | TBCLR | TBIE;

    results[5] = 0x600D;
#ifndef SLEEP
#define SLEEP LPM0_bits
#endif
    __bis_SR_register(SLEEP | GIE);
    for (;;) ;
}

__attribute__((interrupt(TIMER0_B0_VECTOR))) void tb0_ccr0(void) {
    TB0CCR0 += 3277;
    results[0]++;
}

__attribute__((interrupt(TIMER0_B1_VECTOR))) void tb0_shared(void) {
    switch (TB0IV) {
        case TB0IV_TBCCR1:
            results[3] = TB0CCR1;
            results[4]++;
            break;
    }
}

__attribute__((interrupt(TIMER3_B1_VECTOR))) void tb3_shared(void) {
    switch (TB3IV) {
        case TBIV_2:  // CCR1
            results[1]++;
            break;
        case TBIV_14:  // TBIFG
            results[2]++;
            break;
    }
}
