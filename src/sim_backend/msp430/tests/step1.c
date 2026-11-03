/* Step 1 test firmware for the PICSimLab MSP430FR2355 model.
 *
 * results[] (in RAM, at the address msp430-nm reports) holds what the run did:
 *   [0] 0x600D once the arithmetic self-check passed, 0xBAD0 + n if check n failed
 *   [1] P1.1 falling-edge interrupts taken (the runner pulls P1.1 low and high again)
 *   [2] WDT interval interrupts taken (ACLK / 8192 = 4 per second)
 *   [3] times main woke from LPM3
 * P1.0 toggles each time main wakes.
 */
#include <msp430.h>
#include <stdint.h>

volatile uint16_t results[4];

static uint32_t mix(uint32_t a, uint16_t b) {
    return (a << 3) + (a >> 2) - b;
}

static int self_check(void) {
    volatile uint32_t x = 0x12345678;
    uint32_t acc = 0;
    for (uint16_t i = 0; i < 10; i++)
        acc = mix(acc ^ x, i);
    if (acc != 0x52C12443u)
        return 1;
    volatile int16_t a = -1234, b = 77;
    if ((int16_t)(a + b) != -1157)
        return 2;
    if ((a >> 2) != -309)
        return 3;
    static const uint8_t table[5] = {3, 1, 4, 1, 5};
    uint16_t sum = 0;
    for (int i = 0; i < 5; i++)
        sum += table[i];
    if (sum != 14)
        return 4;
    return 0;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    P1DIR |= BIT0;
    P1OUT &= ~BIT0;
    P1DIR &= ~BIT1;   // P1.1: button input with pull-up, falling edge interrupt
    P1REN |= BIT1;
    P1OUT |= BIT1;
    P1IES |= BIT1;
    P1IFG = 0;
    P1IE |= BIT1;

    int bad = self_check();
    results[0] = bad ? 0xBAD0 + bad : 0x600D;

    WDTCTL = WDTPW | WDTSSEL__ACLK | WDTTMSEL | WDTCNTCL | WDTIS__8192;  // 4 interrupts a second
    SFRIE1 |= WDTIE;

    for (;;) {
        __bis_SR_register(LPM3_bits | GIE);
        results[3]++;
        P1OUT ^= BIT0;
    }
}

__attribute__((interrupt(PORT1_VECTOR))) void port1_isr(void) {
    switch (P1IV) {
        case P1IV__P1IFG1:
            results[1]++;
            __bic_SR_register_on_exit(LPM3_bits);
            break;
    }
}

__attribute__((interrupt(WDT_VECTOR))) void wdt_isr(void) {
    results[2]++;
    __bic_SR_register_on_exit(LPM3_bits);
}
